using EasyHook;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace MGSHook
{
    public class CutsceneLauncher : IEntryPoint
    {
        private static IntPtr cutsceneDllPointer;

        static CutsceneLauncher()
        {
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;

            string pathToCutsceneDll = Path.Combine(_runningDirectory, "Cutscene.dll");
            //Avoid "FileNotFoundException" when the Win32 loader tries to load cutscene.dll
            if (File.Exists(pathToCutsceneDll))
            {
                cutsceneDllPointer = NativeMethods.LoadLibrary(pathToCutsceneDll);
            }
        }

        private static void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            var ex = e.ExceptionObject as Exception;
            if(ex != null)
            {
                MessageBox.Show(ex.Message, ex.Source);
            }
            Process.GetCurrentProcess().CloseMainWindow();
            Environment.Exit(1);
        }

        private static string _runningDirectory = Environment.CurrentDirectory;
        IpcInterface _ipcInterface;

        #region CreateFile
        LocalHook _createFileLocalHook;

        public static IntPtr CreateFileHookMethod(
            [MarshalAs(UnmanagedType.LPStr)] string filename,
            [MarshalAs(UnmanagedType.U4)] FileAccess access,
            [MarshalAs(UnmanagedType.U4)] FileShare share,
            IntPtr securityAttributes,
            [MarshalAs(UnmanagedType.U4)] FileMode creationDisposition,
            [MarshalAs(UnmanagedType.U4)] FileAttributes flagsAndAttributes,
            IntPtr templateFile)
        {
            TryPlayVideo(filename);

            return CreateFile(
                filename,
                access,
                share,
                securityAttributes,
                creationDisposition,
                flagsAndAttributes,
                templateFile);
        }

        private static string lastvid = "";

        private static void TryPlayVideo(string filename)
        {
            if (string.IsNullOrWhiteSpace(filename))
            {
                return;
            }
            
            if (filename.ToLower().Contains("movie") && Path.GetExtension(filename.ToLower()) == ".ddv")
            {
                string wmvfilename = filename.ToLower();
                wmvfilename = Path.GetFileNameWithoutExtension(wmvfilename);
                wmvfilename += ".wmv";
                wmvfilename = Path.Combine(_runningDirectory, "movie", wmvfilename);

                if (File.Exists(wmvfilename))
                {
                    if(lastvid != wmvfilename)
                    {
                        lastvid = wmvfilename;
                        NativeMethods.ShowWindow(Process.GetCurrentProcess().MainWindowHandle, NativeMethods.SW_MINIMIZE);
                        Task.Factory.StartNew(() =>
                        {
                            PlayVideo(wmvfilename, Process.GetCurrentProcess().Handle, Process.GetCurrentProcess().MainWindowHandle);
                        });
                    }
                }
            }
        }

        [UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true, CharSet = CharSet.Ansi)]
        public delegate IntPtr CreateFileDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string filename,
            [MarshalAs(UnmanagedType.U4)] FileAccess access,
            [MarshalAs(UnmanagedType.U4)] FileShare share,
            IntPtr securityAttributes,
            [MarshalAs(UnmanagedType.U4)] FileMode creationDisposition,
            [MarshalAs(UnmanagedType.U4)] FileAttributes flagsAndAttributes,
            IntPtr templateFile);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
        public static extern IntPtr CreateFile(
            [MarshalAs(UnmanagedType.LPStr)] string filename,
            [MarshalAs(UnmanagedType.U4)] FileAccess access,
            [MarshalAs(UnmanagedType.U4)] FileShare share,
            IntPtr securityAttributes,
            [MarshalAs(UnmanagedType.U4)] FileMode creationDisposition,
            [MarshalAs(UnmanagedType.U4)] FileAttributes flagsAndAttributes,
            IntPtr templateFile);

        #endregion CreateFile

        [DllImport("Cutscene.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern long PlayVideo([MarshalAs(UnmanagedType.LPTStr)] string filename, IntPtr processHandle, IntPtr gameWindow);

        public CutsceneLauncher(RemoteHooking.IContext inContext, String inChannelName)
        {
            // connect to host...
            _ipcInterface = RemoteHooking.IpcConnectClient<IpcInterface>(inChannelName);
            _ipcInterface.Ping();
        }

        public void Run(RemoteHooking.IContext inContext, String inChannelName)
        {
            // install hook...
            try
            {
                _runningDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

                IntPtr createFileProcAddress = LocalHook.GetProcAddress("kernel32.dll", "CreateFileA");

                _createFileLocalHook = LocalHook.Create(
                    createFileProcAddress,
                    new CreateFileDelegate(CreateFileHookMethod),
                    this);

                _createFileLocalHook.ThreadACL.SetExclusiveACL(new int[] { 0 });
            }
            catch (Exception exception)
            {
                _ipcInterface.ReportException(exception);
                if(cutsceneDllPointer != null)
                {
                    NativeMethods.FreeLibrary(cutsceneDllPointer);
                }
                return;
            }

            _ipcInterface.NotifySucessfulInstallation(RemoteHooking.GetCurrentProcessId());
            RemoteHooking.WakeUpProcess();

            // wait until we are not needed anymore...
            try
            {
                while (true)
                {
                    _ipcInterface.OnHooking();
                }
            }
            catch
            {
                // Ping() will raise an exception if host is unreachable
            }
        }
    }
}
