using EasyHook;
using MGSHook;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Remoting;
using System.Text;
using System.Windows;

namespace MGSLauncher
{
    public static class HookLauncher
    {
        private static string _targetExe = "";
        private static String ChannelName;

        public static void Run(string exeName)
        {

            _targetExe = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), exeName);
            Int32 targetPid = 0;
            try
            {
                if (!File.Exists(_targetExe))
                {
                    Debug.WriteLine(String.Format("{0} introuvable !", _targetExe));
                    Debug.WriteLine("Appuyez sur une touche pour quitter... ");
                }

                RemoteHooking.IpcCreateServer<IpcInterface>(ref ChannelName, WellKnownObjectMode.SingleCall);
                string injectionLibrary = Path.Combine(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location), "MGSHook.dll");
                RemoteHooking.CreateAndInject(_targetExe, "", 0, InjectionOptions.DoNotRequireStrongName, injectionLibrary, injectionLibrary, out targetPid, ChannelName);
                Debug.WriteLine(String.Format("Metal Gear Solid lancé avec succès. PID : {0}", targetPid));
            }
            catch (Exception exception)
            {
                Debug.WriteLine("Erreur grave apparue lors du lancement de Metal Gear Solid \r\n{0}", exception.ToString());
                Debug.WriteLine("Appuyez sur une touche pour quitter... ");
            }

        }
    }
}
