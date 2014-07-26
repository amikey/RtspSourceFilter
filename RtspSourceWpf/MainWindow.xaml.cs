using System.Windows;
using RtspSourceWpf.Enums;

namespace RtspSourceWpf
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private RtspPlayer _player;

        public MainWindow()
        {
            InitializeComponent();
        }

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            if (_player != null)
            {
                videoGrid.Children.Remove(_player);
                _player.Dispose();
            }

            base.OnClosing(e);
        }

        private void PlayClicked(object sender, RoutedEventArgs e)
        {
            if (_player != null)
            {
                videoGrid.Children.Remove(_player);
                _player.Dispose();
            }

            _player = new RtspPlayer(EDecoderType.MicrosoftVideoDecoder,
                EVideoRendererType.EnhancedVideoRenderer);
            videoGrid.Children.Add(_player);
            _player.VerticalContentAlignment = System.Windows.VerticalAlignment.Stretch;
            _player.HorizontalContentAlignment = System.Windows.HorizontalAlignment.Stretch;
            _player.Loaded += player_Loaded;
        }

        private void player_Loaded(object sender, RoutedEventArgs e)
        {
            _player.Connect(RtspUrlTextBox.Text);
            _player.Play();
        }

        private void StopClicked(object sender, RoutedEventArgs e)
        {
            if (_player != null)
            {
                videoGrid.Children.Remove(_player);
                _player.Dispose();
            }
        }
    }
}
