﻿using System;
using System.Linq;
using System.Collections;
using System.IO;
using System.Xml;
using System.Xml.Serialization;

namespace TabletDriverGUI
{
    [XmlRootAttribute("Configuration", IsNullable = true)]
    public class Configuration
    {
        public int ConfigVersion;
        public Area TabletArea;
        public Area TabletFullArea;
        public bool ForceAspectRatio;
        public double Rotation;
        public bool Invert;
        public bool ForceFullArea;
        public OutputModes OutputMode;
        public enum OutputModes
        {
            Absolute = 0,
            Relative = 1,
            Digitizer = 2
        }

        public Area ScreenArea;

        // Smoothing filter
        public double SmoothingLatency;
        public int SmoothingInterval;
        public bool SmoothingEnabled;

        // Noise filter
        public int NoiseFilterBuffer;
        public double NoiseFilterThreshold;
        public bool NoiseFilterEnabled;

        // Anti-smoothing filter
        public double AntiSmoothingShape;
        public double AntiSmoothingCompensation;
        public bool AntiSmoothingIgnoreWhenDragging;
        public bool AntiSmoothingEnabled;

        public Area DesktopSize;
        public bool AutomaticDesktopSize;

        [XmlArray("ButtonMap")]
        [XmlArrayItem("Button")]
        public int[] ButtonMap;
        public bool DisableButtons;

        [XmlArray("CommandsAfter")]
        [XmlArrayItem("Command")]
        public string[] CommandsAfter;

        [XmlArray("CommandsBefore")]
        [XmlArrayItem("Command")]
        public string[] CommandsBefore;

        public int WindowWidth;
        public int WindowHeight;

        public bool AutomaticRestart;
        public bool RunAtStartup;

        public string DriverPath;
        public string DriverArguments;

        public bool DebuggingEnabled;
        public bool DeveloperMode;


        public Configuration()
        {
            ConfigVersion = 1;

            // Screen Map
            ScreenArea = new Area(0, 0, 0, 0);

            // Tablet area
            TabletArea = new Area(80, 45, 40, 22.5);
            TabletFullArea = new Area(100, 50, 50, 25);
            ForceFullArea = true;
            OutputMode = 0;
            ForceAspectRatio = true;
            Rotation = 0;

            DesktopSize = new Area(0, 0, 0, 0);
            AutomaticDesktopSize = true;

            ButtonMap = new int[] { 1, 2, 3 };
            DisableButtons = false;

            SmoothingEnabled = false;
            SmoothingLatency = 0;
            SmoothingInterval = 4;

            NoiseFilterEnabled = false;
            NoiseFilterBuffer = 10;
            NoiseFilterThreshold = 0.5;

            AntiSmoothingEnabled = false;
            AntiSmoothingShape = 0.5;
            AntiSmoothingCompensation = 4.0;

            CommandsAfter = new string[] { "" };
            CommandsBefore = new string[] { "" };

            WindowWidth = 700;
            WindowHeight = 710;

            AutomaticRestart = true;
            RunAtStartup = false;

            DriverPath = "bin/TabletDriverService.exe";
            DriverArguments = "config/init.cfg";

            DebuggingEnabled = false;
            DeveloperMode = false;
        }


        public void Write(string filename)
        {
            var fileWriter = new StreamWriter(filename);

            XmlSerializer serializer = new XmlSerializer(typeof(Configuration));
            XmlWriterSettings xmlWriterSettings = new XmlWriterSettings() { Indent = true };
            XmlWriter writer = XmlWriter.Create(fileWriter, xmlWriterSettings);
            try
            {
                serializer.Serialize(writer, this);
            }
            catch (Exception)
            {
                fileWriter.Close();
                throw;
            }
            fileWriter.Close();
        }

        public static Configuration CreateFromFile(string filename)
        {
            Configuration config = null;
            var serializer = new XmlSerializer(typeof(Configuration));
            var settings = new XmlWriterSettings() { Indent = true };
            var reader = XmlReader.Create(filename);

            try
            {
                config = (Configuration)serializer.Deserialize(reader);
            }
            catch (Exception)
            {
                reader.Close();
                throw;
            }
            reader.Close();
            return config;
        }
    }


}
