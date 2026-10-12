Option Explicit On
Option Strict On

Imports System
Imports System.Collections.Generic
Imports System.Globalization
Imports System.IO
Imports System.Linq
Imports System.Text
Imports System.Text.RegularExpressions

Public Module Materializer

    Private ReadOnly DefaultRoot As String = AppDomain.CurrentDomain.BaseDirectory
    Private ReadOnly LogFilePath As String = Path.Combine(DefaultRoot, "materializer.log")
    Private ReadOnly ReportFilePath As String = Path.Combine(DefaultRoot, "materializer-report.txt")

    Public Sub Main()
        Dim args As String() = Environment.GetCommandLineArgs()
        Dim rootDirectory As String = DefaultRoot

        If args.Length > 1 AndAlso Not String.IsNullOrWhiteSpace(args(1)) Then
            rootDirectory = Path.GetFullPath(args(1))
        End If

        If Not Directory.Exists(rootDirectory) Then
            Console.WriteLine("[Materializer] Root directory does not exist: " & rootDirectory)
            Return
        End If

        Dim configFile As String = Path.Combine(rootDirectory, "sub.json")
        Dim config As Dictionary(Of String, String) = New Dictionary(Of String, String)(StringComparer.OrdinalIgnoreCase)

        If File.Exists(configFile) Then
            config = TryLoadConfiguration(configFile)
            Console.WriteLine("[Materializer] Loaded configuration from: " & configFile)
        Else
            Console.WriteLine("[Materializer] No sub.json found. Using defaults.")
            config("root") = rootDirectory
            config("mode") = "default"
        End If

        Dim inventory As New List(Of String)()
        Dim visitedDirectories As New HashSet(Of String)(StringComparer.OrdinalIgnoreCase)
        Dim processedFiles As Integer = 0
        Dim totalSize As Long = 0

        Try
            ProcessDirectory(rootDirectory, inventory, visitedDirectories, processedFiles, totalSize)
        Catch ex As Exception
            inventory.Add("[ERROR] " & ex.Message)
            WriteLog(inventory)
            Console.WriteLine("[Materializer] Failed to process directory: " & ex.Message)
            Return
        End Try

        Dim reportBuilder As New StringBuilder()
        reportBuilder.AppendLine("Materializer Report")
        reportBuilder.AppendLine("==================")
        reportBuilder.AppendLine("Scan Root: " & rootDirectory)
        reportBuilder.AppendLine("Generated: " & DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture))
        reportBuilder.AppendLine("Configuration Mode: " & config.GetValueOrDefault("mode", "default"))
        reportBuilder.AppendLine("Directories Visited: " & visitedDirectories.Count)
        reportBuilder.AppendLine("Files Processed: " & processedFiles)
        reportBuilder.AppendLine("Total Size: " & FormatBytes(totalSize))
        reportBuilder.AppendLine()

        For Each entry In inventory.OrderBy(Function(x) x, StringComparer.OrdinalIgnoreCase)
            reportBuilder.AppendLine(entry)
        Next

        File.WriteAllText(ReportFilePath, reportBuilder.ToString(), Encoding.UTF8)
        WriteLog(inventory)

        Console.WriteLine("[Materializer] Completed successfully.")
        Console.WriteLine("[Materializer] Report saved to: " & ReportFilePath)
    End Sub

    Private Sub ProcessDirectory(ByVal rootDirectory As String,
                                ByVal inventory As List(Of String),
                                ByVal visitedDirectories As HashSet(Of String),
                                ByRef processedFiles As Integer,
                                ByRef totalSize As Long)

        If Not visitedDirectories.Add(rootDirectory) Then
            Return
        End If

        Dim directories As String() = Directory.GetDirectories(rootDirectory)
        Dim files As String() = Directory.GetFiles(rootDirectory)

        For Each directory In directories
            Dim normalizedDirectory As String = Path.GetFullPath(directory)
            inventory.Add("[DIRECTORY] " & GetRelativePath(rootDirectory, normalizedDirectory))
            ProcessDirectory(normalizedDirectory, inventory, visitedDirectories, processedFiles, totalSize)
        Next

        For Each file In files
            Dim fileInfo As New FileInfo(file)
            processedFiles += 1
            totalSize += fileInfo.Length

            Dim relativePath As String = GetRelativePath(rootDirectory, fileInfo.FullName)
            Dim fileEntry As String = String.Format(CultureInfo.InvariantCulture,
                "[FILE] {0} | {1} | {2}",
                relativePath,
                fileInfo.Length,
                fileInfo.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture))

            inventory.Add(fileEntry)
        Next
    End Sub

    Private Function TryLoadConfiguration(configFile As String) As Dictionary(Of String, String)
        Dim config As New Dictionary(Of String, String)(StringComparer.OrdinalIgnoreCase)

        Try
            Dim rawText As String = File.ReadAllText(configFile)
            Dim pattern As String = """(?<key>[^""\r\n]+)""\s*:\s*(?:""(?<value>[^""\r\n]*)""|(?<number>\d+)|(?<boolean>true|false)|(?<null>null))"
            Dim matches As MatchCollection = Regex.Matches(rawText, pattern, RegexOptions.IgnoreCase)

            For Each match As Match In matches
                Dim key As String = match.Groups("key").Value.Trim()
                Dim value As String = ""

                If match.Groups("value").Success Then
                    value = match.Groups("value").Value.Trim()
                ElseIf match.Groups("number").Success Then
                    value = match.Groups("number").Value.Trim()
                ElseIf match.Groups("boolean").Success Then
                    value = match.Groups("boolean").Value.Trim()
                ElseIf match.Groups("null").Success Then
                    value = "null"
                End If

                If Not String.IsNullOrWhiteSpace(key) Then
                    config(key) = value
                End If
            Next
        Catch ex As Exception
            config("mode") = "error"
            config("error") = ex.Message
        End Try

        Return config
    End Function

    Private Function GetRelativePath(rootDirectory As String, fullPath As String) As String
        Dim root As String = Path.GetFullPath(rootDirectory).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
        Dim target As String = Path.GetFullPath(fullPath)
        Dim relativePath As String = target.Substring(root.Length).TrimStart(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)

        If String.IsNullOrEmpty(relativePath) Then
            Return "."
        End If

        Return relativePath.Replace("\\", "/")
    End Function

    Private Function FormatBytes(value As Long) As String
        Dim units As String() = {"B", "KB", "MB", "GB", "TB"}
        Dim size As Decimal = value
        Dim unitIndex As Integer = 0

        While size >= 1024D AndAlso unitIndex < units.Length - 1
            size /= 1024D
            unitIndex += 1
        End While

        Return String.Format(CultureInfo.InvariantCulture, "{0:0.##} {1}", size, units(unitIndex))
    End Function

    Private Sub WriteLog(entries As IEnumerable(Of String))
        Dim builder As New StringBuilder()
        For Each entry In entries
            builder.AppendLine(DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture) & " - " & entry)
        Next
        File.AppendAllText(LogFilePath, builder.ToString(), Encoding.UTF8)
    End Sub

End Module