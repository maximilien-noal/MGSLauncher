using System;
using System.Diagnostics;
using System.Windows;

namespace MGSLauncher
{
    public partial class MainWindow : Window
    {
        public bool AreOptionsNotVisible
        {
            get { return (bool)GetValue(AreOptionsNotVisibleProperty); }
            set { SetValue(AreOptionsNotVisibleProperty, value); }
        }
        public static readonly DependencyProperty AreOptionsNotVisibleProperty =
            DependencyProperty.Register("AreOptionsNotVisible", typeof(bool), typeof(MainWindow), new PropertyMetadata(true));

        public bool AreOptionsVisible
        {
            get { return (bool)GetValue(AreOptionsVisibleProperty); }
            set { SetValue(AreOptionsVisibleProperty, value); }
        }
        public static readonly DependencyProperty AreOptionsVisibleProperty =
            DependencyProperty.Register("AreOptionsVisible", typeof(bool), typeof(MainWindow), new PropertyMetadata(false));

        public bool IsDgVoodoo2Activated
        {
            get { return (bool)GetValue(IsDgVoodoo2ActivatedProperty); }
            set { SetValue(IsDgVoodoo2ActivatedProperty, value); }
        }
        public static readonly DependencyProperty IsDgVoodoo2ActivatedProperty =
            DependencyProperty.Register("IsDgVoodoo2Activated", typeof(bool), typeof(MainWindow), new PropertyMetadata(Properties.Settings.Default.IsDgVoodoo2Activated));

        public bool IsVideoFixActivated
        {
            get { return (bool)GetValue(IsVideoFixActivatedProperty); }
            set { SetValue(IsVideoFixActivatedProperty, value); }
        }
        public static readonly DependencyProperty IsVideoFixActivatedProperty =
            DependencyProperty.Register("IsVideoFixActivated", typeof(bool), typeof(MainWindow), new PropertyMetadata(Properties.Settings.Default.IsVideoFixActivated));

        public bool IsSweetFXActivated
        {
            get { return (bool)GetValue(IsSweetFXActivatedProperty); }
            set { SetValue(IsSweetFXActivatedProperty, value); }
        }
        public static readonly DependencyProperty IsSweetFXActivatedProperty =
            DependencyProperty.Register("IsSweetFXActivated", typeof(bool), typeof(MainWindow), new PropertyMetadata(Properties.Settings.Default.IsSweetFXActivated));

        public DelegateCommand Play { get; private set; }
        public DelegateCommand PlayVR { get; private set; }
        public DelegateCommand Quit { get; private set; }
        public DelegateCommand ShowOptions { get; private set; }

        public MainWindow()
        {
            Play = new DelegateCommand(Play_Execute);
            PlayVR = new DelegateCommand(PlayVR_Execute);
            Quit = new DelegateCommand(Quit_Execute);
            ShowOptions = new DelegateCommand(ShowOptions_Execute);

            InitializeComponent();

            Application.Current.Exit += OnApplicationExit;
        }

        private void OnApplicationExit(object sender, ExitEventArgs e)
        {
            SaveOptions();
        }

        private void SaveOptions()
        {
            Properties.Settings.Default.IsDgVoodoo2Activated = IsDgVoodoo2Activated;
            Properties.Settings.Default.IsVideoFixActivated = IsVideoFixActivated;
            Properties.Settings.Default.IsSweetFXActivated = IsSweetFXActivated;
            Properties.Settings.Default.Save();
        }

        private void Quit_Execute(object parameters)
        {
            Application.Current.Shutdown();
        }

        private void PlayVR_Execute(object parameters)
        {
            LaunchGame("MGSVR.EXE");
        }

        private void Play_Execute(object parameters)
        {
            LaunchGame("MGSI.EXE");
        }

        private void LaunchGame(string executable)
        {
            try
            {
                SaveOptions();
                OptionsActivator.ApplyOptions();

                if (IsVideoFixActivated)
                {
                    HookLauncher.Run(executable);
                    this.WindowState = WindowState.Minimized;
                }
                else
                {
                    Process.Start(executable);
                    Application.Current.Shutdown();
                }
            }
            catch (Exception e)
            {
                this.WindowState = WindowState.Normal;
                Debug.WriteLine(e.Message);
            }
        }

        private void ShowOptions_Execute(object parameters)
        {
            if(AreOptionsNotVisible)
            {
                SetCurrentValue(AreOptionsVisibleProperty, true);
                SetCurrentValue(AreOptionsNotVisibleProperty, false);
            }
            else
            {
                SetCurrentValue(AreOptionsVisibleProperty, false);
                SetCurrentValue(AreOptionsNotVisibleProperty, true);
            }
        }
    }
}
