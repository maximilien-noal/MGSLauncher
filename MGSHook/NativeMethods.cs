using System;
using System.Runtime.InteropServices;

namespace MGSHook
{
    public static class NativeMethods
    {
        //From WinUser.h
        public const int SW_MINIMIZE = 6;

        [DllImport("User32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmd);

        [DllImport("kernel32.dll")]
        public static extern IntPtr LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll")]
        public static extern bool FreeLibrary(IntPtr hModule);
    }
}
