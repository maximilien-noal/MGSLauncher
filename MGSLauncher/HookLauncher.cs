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

        public static Process Run(string exeName)
        {
            _targetExe = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), exeName);
            Int32 targetPid = 0;
            if (!File.Exists(_targetExe))
            {
                throw new FileNotFoundException(_targetExe);
            }

            RemoteHooking.IpcCreateServer<IpcInterface>(ref ChannelName, WellKnownObjectMode.SingleCall);
            string injectionLibrary = Path.Combine(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location), "MGSHook.dll");
            RemoteHooking.CreateAndInject(_targetExe, "", 0, InjectionOptions.DoNotRequireStrongName, injectionLibrary, injectionLibrary, out targetPid, ChannelName);

            var gameProcess = Process.GetProcessById(targetPid);
            return gameProcess;

#if(DEBUG)
            Debug.WriteLine(String.Format("Metal Gear Solid launched and injected successfully. PID : {0}", targetPid));
#endif
        }
    }
}
