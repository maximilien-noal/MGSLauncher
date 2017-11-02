using System;
using System.IO;
using System.Linq;
using System.Windows;

namespace MGSLauncher
{
    internal static class OptionsActivator
    {
        private static string _gameDir = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);

        public static void ApplyOptions()
        {
            if(Properties.Settings.Default.IsVideoFixActivated)
            {
                RemoveDDVMovies();
            }
            else
            {
                RestoreDDVMovies();
            }
            /*
            if (Properties.Settings.Default.IsDgVoodoo2Activated)
            {
                EnableDgVoodoo2();
            }
            else
            {
                DisableDgVoodoo2();
            }

            if (Properties.Settings.Default.IsReShadeActivated)
            {
                EnableReShade();
            }
            else
            {
                DisableReShade();
            }
            */

        }

        private static void RemoveDDVMovies()
        {
            try
            {
                var movieFiles = Directory.GetFiles(Path.Combine(_gameDir, "movie"), "*.ddv");

                if (movieFiles == null)
                {
                    return;
                }

                foreach(string file in movieFiles)
                {
                    try
                    {
                        string destPath = Path.Combine(_gameDir, Path.GetFileName(file));

                        if (File.Exists(destPath))
                        {
                            File.Delete(destPath);
                        }

                        File.Move(file, destPath);
                    }
                    catch
                    {

                    }
                    
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(String.Format("{0}:{1}{2}", e.Message, Environment.NewLine, e.StackTrace), "Error enabling dgVoodoo2");
            }
        }

        private static void RestoreDDVMovies()
        {
            try
            {
                var movieFiles = Directory.GetFiles(_gameDir, "*.ddv");

                if (movieFiles == null)
                {
                    return;
                }

                foreach (string file in movieFiles)
                {
                    try
                    {
                        string destPath = Path.Combine(_gameDir, "movie", Path.GetFileName(file));

                        if (File.Exists(destPath))
                        {
                            continue;
                        }

                        File.Move(file, destPath);
                    }
                    catch
                    {

                    }
                    
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(String.Format("{0}:{1}{2}", e.Message, Environment.NewLine, e.StackTrace), "Error enabling dgVoodoo2");
            }
        }

        private static void EnableDgVoodoo2()
        {
            try
            {
                string saveDgVoodoo2DirPath = Path.Combine(_gameDir, "dgvoodoo2");

                string ddrawPath = Path.Combine(_gameDir, "ddraw.dll");
                string ddrawSavePath = Path.Combine(saveDgVoodoo2DirPath, "ddraw.dll");
                string d3DImmPath = Path.Combine(_gameDir, "D3DImm.dll");
                string d3DImmSavePath = Path.Combine(saveDgVoodoo2DirPath, "D3DImm.dll");
                string d3d8Path = Path.Combine(_gameDir, "d3d8.dll");
                string d3d8SavePath = Path.Combine(saveDgVoodoo2DirPath, "d3d8.dll");

                if (File.Exists(ddrawSavePath) && File.Exists(ddrawPath) == false)
                {
                    File.Move(ddrawSavePath, ddrawPath);
                }

                if (File.Exists(d3DImmSavePath) && File.Exists(d3DImmPath) == false)
                {
                    File.Move(d3DImmSavePath, d3DImmPath);
                }

                if (File.Exists(d3d8SavePath) && File.Exists(d3d8Path) == false)
                {
                    File.Move(d3d8SavePath, d3d8Path);
                }
            }
            catch(Exception e)
            {
                MessageBox.Show(String.Format("{0}:{1}{2}", e.Message, Environment.NewLine, e.StackTrace), "Error enabling dgVoodoo2");
            }
        }

        private static void DisableDgVoodoo2()
        {
            try
            {
                string saveDgVoodoo2DirPath = Path.Combine(_gameDir, "dgvoodoo2");
                if(Directory.Exists(saveDgVoodoo2DirPath) == false)
                {
                    Directory.CreateDirectory(saveDgVoodoo2DirPath);
                }

                string ddrawPath = Path.Combine(_gameDir, "ddraw.dll");
                string ddrawSavePath = Path.Combine(saveDgVoodoo2DirPath, "ddraw.dll");
                string d3DImmPath = Path.Combine(_gameDir, "D3DImm.dll");
                string d3DImmSavePath = Path.Combine(saveDgVoodoo2DirPath, "D3DImm.dll");
                string d3d8Path = Path.Combine(_gameDir, "d3d8.dll");
                string d3d8SavePath = Path.Combine(saveDgVoodoo2DirPath, "d3d8.dll");

                if(File.Exists(ddrawPath))
                {
                    if (File.Exists(ddrawSavePath))
                    {
                        File.Delete(ddrawSavePath);
                    }
                    File.Move(ddrawPath, ddrawSavePath);
                }

                if (File.Exists(d3DImmPath))
                {
                    if (File.Exists(d3DImmSavePath))
                    {
                        File.Delete(d3DImmSavePath);
                    }
                    File.Move(d3DImmPath, d3DImmSavePath);
                }

                if (File.Exists(d3d8Path))
                {
                    if (File.Exists(d3d8SavePath))
                    {
                        File.Delete(d3d8SavePath);
                    }
                    File.Move(d3d8Path, d3d8SavePath);
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(String.Format("{0}:{1}{2}", e.Message, Environment.NewLine, e.StackTrace), "Error disabling dgVoodoo2");
            }
        }

        private static void EnableReShade()
        {
            try
            {
                string dxgiSavePath = Path.Combine(_gameDir, @"reshade-shaders\dxgi.dll");
                string dxgiDestPath = Path.Combine(_gameDir, "dxgi.dll");

                if(File.Exists(dxgiSavePath))
                {
                    if (File.Exists(dxgiDestPath))
                    {
                        File.Delete(dxgiDestPath);
                    }
                    File.Move(dxgiSavePath, dxgiDestPath);
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(String.Format("{0}:{1}{2}", e.Message, Environment.NewLine, e.StackTrace), "Error enabling ReShade");
            }
        }

        private static void DisableReShade()
        {
            try
            {
                string dxgiSavePath = Path.Combine(_gameDir, @"reshade-shaders\dxgi.dll");
                string dxgiSourcePath = Path.Combine(_gameDir, "dxgi.dll");
                

                if (File.Exists(dxgiSourcePath))
                {
                    if (File.Exists(dxgiSavePath))
                    {
                        File.Delete(dxgiSavePath);
                    }
                    File.Move(dxgiSourcePath, dxgiSavePath);
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(String.Format("{0}:{1}{2}", e.Message, Environment.NewLine, e.StackTrace), "Error disabling ReShade");
            }
        }
    }
}
