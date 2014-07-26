#include "H264StreamParser.h"

#include "BitVector.hh"
#include "H264or5VideoStreamFramer.hh"
#define SPS_MAX_SIZE 1000

namespace
{
    const uint8_t subWidthChroma[] = {1, 2, 2, 1};
    const uint8_t subHeightChroma[] = {1, 2, 1, 1};
}

H264StreamParser::H264StreamParser(uint8_t* sps, unsigned spsSize)
    : _sps(SPS_MAX_SIZE), _width(0), _height(0), _framerate(0)
{
    spsSize = removeH264or5EmulationBytes(_sps.data(), _sps.size(), sps, spsSize);

    BitVector bv(_sps.data(), 0, spsSize * 8);
    bv.skipBits(8); // forbidden_zero_bit; nal_ref_idc; nal_unit_type
    unsigned profile_idc = bv.getBits(8);
    bv.skipBits(8); // 6 constraint_setN_flag and "reserved_zero_2bits" at the end
    bv.skipBits(8); // level_idc
    unsigned seq_parameter_set_id = bv.get_expGolomb();
    unsigned chroma_format_idc = 1;
    Boolean separate_colour_plane_flag = False;
    if (profile_idc == 100 || // High profile
        profile_idc == 110 || // High10 profile
        profile_idc == 122 || // High422 profile
        profile_idc == 244 || // High444 Predictive profile
        profile_idc == 44 ||  // Cavlc444 profile
        profile_idc == 83 ||  // Scalable Constrained High profile (SVC)
        profile_idc == 86 ||  // Scalable High Intra profile (SVC)
        profile_idc == 118 || // Stereo High profile (MVC)
        profile_idc == 128 || // Multiview High profile (MVC)
        profile_idc == 138 || // Multiview Depth High profile (MVCD)
        profile_idc == 144)   // old High444 profile
    {
        chroma_format_idc = bv.get_expGolomb();
        if (chroma_format_idc == 3)
            separate_colour_plane_flag = bv.get1BitBoolean();
        bv.get_expGolomb(); // bit_depth_luma_minus8
        bv.get_expGolomb(); // bit_depth_chroma_minus8
        bv.skipBits(1);     // qpprime_y_zero_transform_bypass_flag
        Boolean seq_scaling_matrix_present_flag = bv.get1BitBoolean();
        if (seq_scaling_matrix_present_flag)
        {
            for (int i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); ++i)
            {
                Boolean seq_scaling_list_present_flag = bv.get1BitBoolean();
                if (seq_scaling_list_present_flag)
                {
                    // Decode scaling matrices
                    unsigned sizeOfScalingList = i < 6 ? 16 : 64;
                    unsigned lastScale = 8;
                    unsigned nextScale = 8;
                    for (unsigned j = 0; j < sizeOfScalingList; ++j)
                    {
                        if (nextScale != 0)
                        {
                            unsigned delta_scale = bv.get_expGolomb();
                            nextScale = (lastScale + delta_scale + 256) % 256;
                        }
                        lastScale = (nextScale == 0) ? lastScale : nextScale;
                    }
                }
            }
        }
    }
    unsigned log2_max_frame_num_minus4 = bv.get_expGolomb();
    unsigned pic_order_cnt_type = bv.get_expGolomb();
    if (pic_order_cnt_type == 0)
    {
        bv.get_expGolomb(); // log2_max_pic_order_cnt_lsb_minus4
    }
    else if (pic_order_cnt_type == 1)
    {
        bv.skipBits(1);     // delta_pic_order_always_zero_flag
        bv.get_expGolomb(); // offset_for_non_ref_pic SIGNED!
        bv.get_expGolomb(); // offset_for_top_to_bottom_field SIGNED!
        unsigned num_ref_frames_in_pic_order_cnt_cycle = bv.get_expGolomb();
        for (unsigned i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i)
            bv.get_expGolomb(); // offset_for_ref_frame[i] SIGNED!
    }
    unsigned num_ref_frames = bv.get_expGolomb();
    Boolean gaps_in_frame_num_value_allowed_flag = bv.get1BitBoolean();
    unsigned pic_width_in_mbs_minus1 = bv.get_expGolomb();
    unsigned pic_height_in_map_units_minus1 = bv.get_expGolomb();
    Boolean frame_mbs_only_flag = bv.get1BitBoolean();
    if (!frame_mbs_only_flag)
        bv.skipBits(1); // mb_adaptive_frame_field_flag
    bv.skipBits(1);     // direct_8x8_inference_flag
    Boolean frame_cropping_flag = bv.get1BitBoolean();
    unsigned frame_crop_left_offset = 0, frame_crop_right_offset = 0, frame_crop_top_offset = 0,
             frame_crop_bottom_offset = 0;
    if (frame_cropping_flag)
    {
        frame_crop_left_offset = bv.get_expGolomb();
        frame_crop_right_offset = bv.get_expGolomb();
        frame_crop_top_offset = bv.get_expGolomb();
        frame_crop_bottom_offset = bv.get_expGolomb();
    }

    // Formula taken from MediaInfo
    _width = (pic_width_in_mbs_minus1 + 1) * 16;
    _height = (pic_height_in_map_units_minus1 + 1) * 16 * (2 - frame_mbs_only_flag);
    unsigned chromaArrayType = separate_colour_plane_flag ? 0 : chroma_format_idc;
    unsigned cropUnitX = subWidthChroma[chromaArrayType];
    unsigned cropUnitY = subHeightChroma[chromaArrayType] * (2 - frame_mbs_only_flag);
    _width -= (frame_crop_left_offset + frame_crop_right_offset) * cropUnitX;
    _height -= (frame_crop_top_offset + frame_crop_bottom_offset) * cropUnitY;

    Boolean vui_parameters_present_flag = bv.get1BitBoolean();
    if (vui_parameters_present_flag)
    {
        // Decode VUI
        Boolean aspect_ratio_info_present_flag = bv.get1BitBoolean();
        if (aspect_ratio_info_present_flag)
        {
            unsigned aspect_ratio_idc = bv.getBits(8);
            if (aspect_ratio_idc == 255 /*Extended_SAR*/)
                bv.skipBits(32); // sar_width(16); sar_height(16)
        }
        Boolean overscan_info_present_flag = bv.get1BitBoolean();
        if (overscan_info_present_flag)
            bv.skipBits(1); // overscan_appropriate_flag
        Boolean video_signal_type_present_flag = bv.get1BitBoolean();
        if (video_signal_type_present_flag)
        {
            bv.skipBits(4); // video_format(3); video_full_range_flag(1)
            Boolean colour_description_present_flag = bv.get1BitBoolean();
            if (colour_description_present_flag)
                bv.skipBits(24); // colour_primaries(8); transfer_characteristics(8);
                                 // matrix_coefficients(8)
        }
        Boolean chroma_loc_info_present_flag = bv.get1BitBoolean();
        if (chroma_loc_info_present_flag)
        {
            bv.get_expGolomb(); // chroma_sample_loc_type_top_field
            bv.get_expGolomb(); // chroma_sample_loc_type_bottom_field
        }
        Boolean timing_info_present_flag = bv.get1BitBoolean();
        if (timing_info_present_flag)
        {
            unsigned num_units_in_tick = bv.getBits(32);
            unsigned time_scale = bv.getBits(32);
            Boolean fixed_frame_rate_flag = bv.get1BitBoolean();
            _framerate = (double)time_scale / (double)num_units_in_tick / 2.0;
        }
    }
}