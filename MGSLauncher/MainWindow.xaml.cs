using MGSHook;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Windows;
using System.Windows.Interop;

namespace MGSLauncher
{
    public partial class MainWindow : Window
    {
        public DelegateCommand Play { get; private set; }
        public DelegateCommand PlayVR { get; private set; }
        public DelegateCommand Quit { get; private set; }

        public MainWindow()
        {
            Play = new DelegateCommand(Play_Execute);
            PlayVR = new DelegateCommand(PlayVR_Execute);
            Quit = new DelegateCommand(Quit_Execute);

            InitializeComponent();
        }

        private void Quit_Execute(object parameters)
        {
            Application.Current.Shutdown();
        }

        private void PlayVR_Execute(object parameters)
        {
            try
            {
                HookLauncher.Run("MGSVR.EXE");
                this.WindowState = WindowState.Minimized;
            }
            catch(Exception e)
            {
                this.WindowState = WindowState.Normal;
                Debug.WriteLine(e.Message);
            }
        }

        private void Play_Execute(object parameters)
        {
            try
            {
                HookLauncher.Run("MGSI.EXE");
                this.WindowState = WindowState.Minimized;
            }
            catch(Exception e)
            {
                this.WindowState = WindowState.Normal;
                Debug.WriteLine(e.Message);
            }
        }
    }
}
