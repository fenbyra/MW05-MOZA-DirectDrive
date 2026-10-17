// Transparent compatibility marker, NOT Forza. No network/device/process-query calls.
// A parent-maintained private lease replaces PID/StartTime checks from launcher v1.0.
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
public static class MW05TelemetryMarker {
    static string diagnostic;
    static void Note(string message) {
        try { File.AppendAllText(diagnostic, DateTime.UtcNow.ToString("o")+" "+message+Environment.NewLine); }
        catch { /* Diagnostics must not create an unhandled failure. */ }
    }
    public static int Main(string[] args) {
        if(args.Length!=3) return 2;
        string lease=args[0], expected=args[1]; diagnostic=args[2];
        Note("START lease monitor v1.1; no PID/StartTime query");
        try {
            if(expected.Length!=32 || File.ReadAllText(lease).Trim()!=expected) {
                Note("EXIT invalid lease token"); return 3;
            }
        } catch(Exception ex) { Note("EXIT initial lease read failed: "+ex.GetType().Name+" "+ex.Message); return 3; }
        Stopwatch freshness=Stopwatch.StartNew();
        DateTime previous=DateTime.MinValue;
        bool warned=false;
        Note("READY parent heartbeat required; timeout15seconds; no fixed lifetime");
        while(true) {
            try {
                if(!File.Exists(lease)) { Note("EXIT lease removed; normal shutdown"); return 0; }
                DateTime stamp=File.GetLastWriteTimeUtc(lease);
                if(stamp!=previous) {
                    previous=stamp; freshness.Restart(); warned=false;
                }
            } catch(Exception ex) {
                if(!warned) { Note("WARN lease check: "+ex.GetType().Name+" "+ex.Message); warned=true; }
            }
            if(freshness.Elapsed.TotalSeconds>=15) {
                Note("EXIT parent heartbeat expired; supervisor stopped or stalled"); return 4;
            }
            Thread.Sleep(250);
        }
    }
}
