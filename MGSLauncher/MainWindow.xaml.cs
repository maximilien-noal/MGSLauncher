using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Windows;

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
                System.Diagnostics.Process.Start(System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "MGSVR.EXE"));
                Application.Current.Shutdown();
            }
            catch(Exception e)
            {
                MessageBox.Show(e.Message);
            }

        }

        private void Play_Execute(object parameters)
        {
            try
            {
                System.Diagnostics.Process.Start(System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "MGSI.EXE"));
                Application.Current.Shutdown();
            }
            catch(Exception e)
            {
                MessageBox.Show(e.Message);
            }

        }
    }
}
