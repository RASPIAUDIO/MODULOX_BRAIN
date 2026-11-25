param(
    [ValidateRange(1024, 65535)][int]$Port = 8765,
    [switch]$Serve,
    [switch]$Stop,
    [switch]$NoBrowser,
    [string]$Instance = '',
    [string]$StopToken = ''
)
$ErrorActionPreference = 'Stop'
$stateDirectory = Join-Path $env:LOCALAPPDATA 'Modulox'
$stateFile = Join-Path $stateDirectory "offline-$Port.json"
$baseUrl = "http://127.0.0.1:$Port"

function Get-OfflineStatus {
    try {
        $request = [System.Net.HttpWebRequest]::Create("$baseUrl/__modulox/status")
        $request.Proxy = $null
        $request.Timeout = 800
        $response = $request.GetResponse()
        try {
            $reader = New-Object System.IO.StreamReader($response.GetResponseStream())
            try { return ($reader.ReadToEnd() | ConvertFrom-Json) } finally { $reader.Dispose() }
        } finally { $response.Dispose() }
    } catch { return $null }
}

try {
    if ($Stop) {
        if (!(Test-Path -LiteralPath $stateFile)) { Write-Output 'Aucun serveur Modulox autonome a arreter.'; exit 0 }
        $state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
        $status = Get-OfflineStatus
        if (!$status -or $status.app -ne 'modulox-offline' -or $status.instance -ne $state.instance) {
            Write-Output 'Le serveur Modulox autonome est deja arrete.'; exit 0
        }
        $request = [System.Net.HttpWebRequest]::Create("$baseUrl/__modulox/stop")
        $request.Proxy = $null; $request.Method = 'POST'; $request.ContentLength = 0; $request.Timeout = 3000
        $request.Headers.Add('X-Modulox-Token', [string]$state.token)
        $response = $request.GetResponse(); $response.Dispose()
        Write-Output 'Serveur Modulox arrete.'; exit 0
    }

    if ($Serve) {
        if (!$Instance -or !$StopToken) { throw 'Identifiant de session manquant.' }
        Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Threading;

public sealed class ModuloxOfflineServer {
    readonly TcpListener listener;
    readonly string root, instance, token;
    readonly int port;
    volatile bool running = true;
    int clients;
    public ModuloxOfflineServer(string root, int port, string instance, string token) {
        this.root = Path.GetFullPath(root); this.port = port; this.instance = instance; this.token = token;
        listener = new TcpListener(IPAddress.Loopback, port);
        listener.ExclusiveAddressUse = true;
        listener.Start(64);
    }
    public void Run() {
        try {
            while (running) {
                TcpClient client;
                try { client = listener.AcceptTcpClient(); }
                catch (SocketException) { if (!running) break; throw; }
                if (Interlocked.Increment(ref clients) > 64) { client.Close(); Interlocked.Decrement(ref clients); continue; }
                ThreadPool.QueueUserWorkItem(delegate(object state) {
                    try { Handle((TcpClient)state); } catch { }
                    finally { ((TcpClient)state).Close(); Interlocked.Decrement(ref clients); }
                }, client);
            }
        } finally { listener.Stop(); }
    }
    static byte[] Text(string value) { return Encoding.UTF8.GetBytes(value); }
    static void Send(NetworkStream stream, int status, string mime, byte[] bytes, bool head) {
        string reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found" : status == 405 ? "Method Not Allowed" : "Bad Request";
        byte[] header = Encoding.ASCII.GetBytes("HTTP/1.1 " + status + " " + reason + "\r\nContent-Type: " + mime +
            "\r\nContent-Length: " + bytes.Length + "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n");
        stream.Write(header, 0, header.Length);
        if (!head) stream.Write(bytes, 0, bytes.Length);
        stream.Flush();
    }
    void Handle(TcpClient client) {
        client.ReceiveTimeout = client.SendTimeout = 5000;
        NetworkStream stream = client.GetStream();
        // Read only the bounded request headers. No upload or filesystem-write endpoint exists.
        var header = new MemoryStream(); int previous = 0, end = 0;
        while (header.Length < 16384) {
            int value = stream.ReadByte(); if (value < 0) return; header.WriteByte((byte)value);
            end = (previous == 13 && value == 10) ? end + 1 : (value == 13 ? end : 0);
            previous = value;
            if (end == 2) break;
        }
        if (end != 2) { Send(stream, 400, "text/plain", Text("Bad request"), false); return; }
        string[] lines = Encoding.ASCII.GetString(header.ToArray()).Split(new string[] { "\r\n" }, StringSplitOptions.None);
        string[] request = lines[0].Split(' ');
        if (request.Length != 3) { Send(stream, 400, "text/plain", Text("Bad request"), false); return; }
        var headers = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        for (int i = 1; i < lines.Length; i++) { int colon = lines[i].IndexOf(':'); if (colon > 0) headers[lines[i].Substring(0, colon)] = lines[i].Substring(colon + 1).Trim(); }
        string host; headers.TryGetValue("Host", out host);
        if (host != "127.0.0.1:" + port && host != "localhost:" + port) { Send(stream, 403, "text/plain", Text("Loopback only"), false); return; }
        string path = request[1].Split('?')[0]; bool head = request[0] == "HEAD";
        if (path == "/__modulox/stop" && request[0] == "POST") {
            string supplied;
            if (!headers.TryGetValue("X-Modulox-Token", out supplied) || supplied != token) { Send(stream, 403, "text/plain", Text("Forbidden"), false); return; }
            Send(stream, 200, "text/plain", Text("Stopped"), false);
            running = false; listener.Stop(); return;
        }
        if (request[0] != "GET" && !head) { Send(stream, 405, "text/plain", Text("Read only"), false); return; }
        if (path == "/__modulox/status") { Send(stream, 200, "application/json", Text("{\"app\":\"modulox-offline\",\"version\":1,\"instance\":\"" + instance + "\"}"), head); return; }
        if (path == "/presets/index.json") {
            string[] paths = Directory.GetFiles(Path.Combine(root, "dist", "presets"), "*.json"); Array.Sort(paths, StringComparer.Ordinal);
            var names = new List<string>();
            foreach (string file in paths) { string name = Path.GetFileName(file); if (name != "index.json" && Regex.IsMatch(name, "^[a-z0-9-]+\\.json$")) names.Add("\"" + name + "\""); }
            Send(stream, 200, "application/json", Text("[" + string.Join(",", names.ToArray()) + "]"), head); return;
        }
        string filePath = null, mime = "application/octet-stream";
        if (path == "/") { filePath = Path.Combine(root, "dist", "index.html"); mime = "text/html; charset=utf-8"; }
        else if (Regex.IsMatch(path, "^/modulox-(front|boot|boot-still)\\.webp$")) { filePath = Path.Combine(root, "dist", path.Substring(1)); mime = "image/webp"; }
        else if (Regex.IsMatch(path, "^/[a-z][a-z0-9-]*\\.(mjs|css)$")) { filePath = Path.Combine(root, "dist", path.Substring(1)); mime = path.EndsWith(".css") ? "text/css; charset=utf-8" : "text/javascript; charset=utf-8"; }
        else if (Regex.IsMatch(path, "^/presets/[a-z0-9-]+\\.json$") || path == "/presets/assets/index.json") { filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar)); mime = "application/json"; }
        else if (Regex.IsMatch(path, "^/presets/assets/[a-f0-9]{14}\\.pcm$")) filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar));
        else if (Regex.IsMatch(path, "^/(firmware|factory)/(s3-32m16m/)?[a-z0-9][a-z0-9.-]*\\.(json|bin)$")) { filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar)); mime = path.EndsWith(".json") ? "application/json" : "application/octet-stream"; }
        else if (Regex.IsMatch(path, "^/vendor/esptool/([a-zA-Z0-9_-]+/)*[a-zA-Z0-9_.-]+\\.(js|json)$")) { filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar)); mime = path.EndsWith(".json") ? "application/json" : "text/javascript"; }
        else if (Regex.IsMatch(path, "^/demo-samples/(kick01|kick04|snare02|voice)\\.wav$")) { filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar)); mime = "audio/wav"; }
        else if (Regex.IsMatch(path, "^/locales/(en|es|zh|de|it|pt|ja|ko)\\.json$")) { filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar)); mime = "application/json"; }
        else if (Regex.IsMatch(path, "^/flags/(fr|gb|es|cn|de|it|pt|jp|kr)\\.png$")) { filePath = Path.Combine(root, "dist", path.Substring(1).Replace('/', Path.DirectorySeparatorChar)); mime = "image/png"; }
        else if (Regex.IsMatch(path, "^/examples/(01-basic|02-triple)\\.json$")) {
            filePath = Path.Combine(root, "dist", "examples", Path.GetFileName(path)); mime = "application/json";
            if (!File.Exists(filePath)) filePath = Path.Combine(root, "..", "firmware", "patches", Path.GetFileName(path));
        }
        if (filePath == null || !File.Exists(filePath)) { Send(stream, 404, "text/plain", Text("Not found"), head); return; }
        Send(stream, 200, mime, File.ReadAllBytes(filePath), head);
    }
}
'@
        $server = New-Object ModuloxOfflineServer($PSScriptRoot, $Port, $Instance, $StopToken)
        New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
        @{ instance = $Instance; token = $StopToken; root = $PSScriptRoot; pid = $PID } | ConvertTo-Json | Set-Content -LiteralPath $stateFile -Encoding UTF8
        try { $server.Run() }
        finally {
            if (Test-Path -LiteralPath $stateFile) {
                $savedState = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
                if ($savedState.instance -eq $Instance) { Remove-Item -LiteralPath $stateFile }
            }
        }
        exit 0
    }

    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'dist/index.html'))) { throw 'Le dossier dist manque. Extraire tout le ZIP avant de lancer Modulox.' }
    $browserPath = $null
    if (!$NoBrowser) {
        $candidates = @(
            (Join-Path $env:ProgramFiles 'Google/Chrome/Application/chrome.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'Google/Chrome/Application/chrome.exe'),
            (Join-Path $env:LOCALAPPDATA 'Google/Chrome/Application/chrome.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'Microsoft/Edge/Application/msedge.exe'),
            (Join-Path $env:ProgramFiles 'Microsoft/Edge/Application/msedge.exe')
        )
        $browserPath = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (!$browserPath) { throw 'Chrome ou Edge est necessaire pour utiliser le MIDI USB et les dossiers personnels.' }
    }
    $status = Get-OfflineStatus
    if ($status -and $status.app -eq 'modulox-offline') {
        $state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
        if ($state.instance -ne $status.instance -or $state.root -ne $PSScriptRoot) { throw 'Une autre copie de Modulox est ouverte. Lance arreter.cmd dans cette copie avant de changer de version.' }
    } else {
        $instanceId = [guid]::NewGuid().ToString('N')
        $shutdownToken = [guid]::NewGuid().ToString('N')
        $powershellPath = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
        $scriptPath = Join-Path $PSScriptRoot 'offline.ps1'
        $worker = Start-Process -FilePath $powershellPath -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $scriptPath + '"'), '-Serve', '-Port', $Port, '-Instance', $instanceId, '-StopToken', $shutdownToken) -WindowStyle Hidden -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        do {
            $status = Get-OfflineStatus
            if ($status -and $status.instance -eq $instanceId) { break }
            if ($worker.HasExited) { throw "Impossible de lancer le serveur local. Le port $Port est peut-etre utilise par un autre logiciel ou un ancien serveur Modulox." }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        if (!$status -or $status.instance -ne $instanceId) { throw 'Le serveur local ne repond pas. Relance lancer.cmd.' }
    }
    if (!$NoBrowser) { Start-Process -FilePath $browserPath -ArgumentList @("--app=$baseUrl/", '--new-window') -WindowStyle Normal | Out-Null }
    Write-Output "Modulox disponible sur $baseUrl/ (hors ligne). Pour arreter : arreter.cmd."
} catch {
    if ($Serve) {
        New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
        $_.Exception.Message | Set-Content -LiteralPath (Join-Path $stateDirectory "offline-$Port.log") -Encoding UTF8
    }
    Write-Error $_.Exception.Message
    exit 1
}
