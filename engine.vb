Imports System
Imports System.Collections.Generic
Imports System.IO

''' <summary>Utilities for locating Fabric mods in a Minecraft instance.</summary>
Public Module ModManager

    ''' <summary>
    ''' Creates the instance's mods directory if necessary and returns its JAR files.
    ''' </summary>
    Public Function DiscoverMods(instanceDirectory As String) As IReadOnlyList(Of String)
        If String.IsNullOrWhiteSpace(instanceDirectory) Then
            Throw New ArgumentException("An instance directory is required.", NameOf(instanceDirectory))
        End If

        Dim fullInstancePath As String = Path.GetFullPath(instanceDirectory)
        Dim modsDirectory As String = Path.Combine(fullInstancePath, "mods")
        Directory.CreateDirectory(modsDirectory)

        Dim modFiles As New List(Of String)
        For Each filePath As String In Directory.EnumerateFiles(modsDirectory, "*.jar", SearchOption.TopDirectoryOnly)
            modFiles.Add(filePath)
        Next

        modFiles.Sort(StringComparer.OrdinalIgnoreCase)
        Return modFiles.AsReadOnly()
    End Function

End Module