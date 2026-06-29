# mapgen_gui.ps1
# PowerShell GUI wrapper for the mapgen.exe CLI tool.
# Provides a Windows Forms dialog with input fields for tileset path, output path,
# grid dimensions, and optional seed, then invokes mapgen.exe and displays the result.

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Resolve mapgen.exe relative to this script's location
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$mapgenDebug = Join-Path $scriptDir "..\bin\Debug\mapgen.exe"
$mapgenRelease = Join-Path $scriptDir "..\bin\Release\mapgen.exe"

# Prefer Release build, fall back to Debug
if (Test-Path $mapgenRelease) {
    $mapgenExe = (Resolve-Path $mapgenRelease).Path
} elseif (Test-Path $mapgenDebug) {
    $mapgenExe = (Resolve-Path $mapgenDebug).Path
} else {
    $mapgenExe = $null
}

# --- Form Setup ---
$form = New-Object System.Windows.Forms.Form
$form.Text = "Map Generator"
$form.Size = New-Object System.Drawing.Size(480, 380)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false

$y = 15

# --- Tileset Path ---
$lblTileset = New-Object System.Windows.Forms.Label
$lblTileset.Text = "Tileset Path:"
$lblTileset.Location = New-Object System.Drawing.Point(15, $y)
$lblTileset.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($lblTileset)

$txtTileset = New-Object System.Windows.Forms.TextBox
$txtTileset.Location = New-Object System.Drawing.Point(120, $y)
$txtTileset.Size = New-Object System.Drawing.Size(260, 20)
$form.Controls.Add($txtTileset)

$btnBrowseTileset = New-Object System.Windows.Forms.Button
$btnBrowseTileset.Text = "..."
$btnBrowseTileset.Location = New-Object System.Drawing.Point(385, ($y - 1))
$btnBrowseTileset.Size = New-Object System.Drawing.Size(40, 23)
$btnBrowseTileset.Add_Click({
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = "Select tileset folder"
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtTileset.Text = $dialog.SelectedPath
    }
})
$form.Controls.Add($btnBrowseTileset)

$y += 35

# --- Output Path ---
$lblOutput = New-Object System.Windows.Forms.Label
$lblOutput.Text = "Output Path:"
$lblOutput.Location = New-Object System.Drawing.Point(15, $y)
$lblOutput.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($lblOutput)

$txtOutput = New-Object System.Windows.Forms.TextBox
$txtOutput.Location = New-Object System.Drawing.Point(120, $y)
$txtOutput.Size = New-Object System.Drawing.Size(260, 20)
$form.Controls.Add($txtOutput)

$btnBrowseOutput = New-Object System.Windows.Forms.Button
$btnBrowseOutput.Text = "..."
$btnBrowseOutput.Location = New-Object System.Drawing.Point(385, ($y - 1))
$btnBrowseOutput.Size = New-Object System.Drawing.Size(40, 23)
$btnBrowseOutput.Add_Click({
    $dialog = New-Object System.Windows.Forms.SaveFileDialog
    $dialog.Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*"
    $dialog.DefaultExt = "json"
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtOutput.Text = $dialog.FileName
    }
})
$form.Controls.Add($btnBrowseOutput)

$y += 35

# --- Grid Width ---
$lblWidth = New-Object System.Windows.Forms.Label
$lblWidth.Text = "Grid Width:"
$lblWidth.Location = New-Object System.Drawing.Point(15, $y)
$lblWidth.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($lblWidth)

$numWidth = New-Object System.Windows.Forms.NumericUpDown
$numWidth.Location = New-Object System.Drawing.Point(120, $y)
$numWidth.Size = New-Object System.Drawing.Size(100, 20)
$numWidth.Minimum = 1
$numWidth.Maximum = 1024
$numWidth.Value = 64
$form.Controls.Add($numWidth)

$y += 35

# --- Grid Height ---
$lblHeight = New-Object System.Windows.Forms.Label
$lblHeight.Text = "Grid Height:"
$lblHeight.Location = New-Object System.Drawing.Point(15, $y)
$lblHeight.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($lblHeight)

$numHeight = New-Object System.Windows.Forms.NumericUpDown
$numHeight.Location = New-Object System.Drawing.Point(120, $y)
$numHeight.Size = New-Object System.Drawing.Size(100, 20)
$numHeight.Minimum = 1
$numHeight.Maximum = 1024
$numHeight.Value = 64
$form.Controls.Add($numHeight)

$y += 35

# --- Seed (optional) ---
$lblSeed = New-Object System.Windows.Forms.Label
$lblSeed.Text = "Seed (optional):"
$lblSeed.Location = New-Object System.Drawing.Point(15, $y)
$lblSeed.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($lblSeed)

$txtSeed = New-Object System.Windows.Forms.TextBox
$txtSeed.Location = New-Object System.Drawing.Point(120, $y)
$txtSeed.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($txtSeed)

$y += 35

# --- Status label ---
$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.Text = ""
$lblStatus.Location = New-Object System.Drawing.Point(15, ($y + 40))
$lblStatus.Size = New-Object System.Drawing.Size(430, 60)
$lblStatus.ForeColor = [System.Drawing.Color]::Black
$form.Controls.Add($lblStatus)

# --- mapgen.exe path display ---
$lblExePath = New-Object System.Windows.Forms.Label
if ($mapgenExe) {
    $lblExePath.Text = "Using: $mapgenExe"
    $lblExePath.ForeColor = [System.Drawing.Color]::DarkGreen
} else {
    $lblExePath.Text = "Warning: mapgen.exe not found in bin\Debug or bin\Release"
    $lblExePath.ForeColor = [System.Drawing.Color]::Red
}
$lblExePath.Location = New-Object System.Drawing.Point(15, ($y + 100))
$lblExePath.Size = New-Object System.Drawing.Size(430, 40)
$form.Controls.Add($lblExePath)

# --- Generate Button ---
$btnGenerate = New-Object System.Windows.Forms.Button
$btnGenerate.Text = "Generate"
$btnGenerate.Location = New-Object System.Drawing.Point(180, $y)
$btnGenerate.Size = New-Object System.Drawing.Size(100, 30)
$btnGenerate.Add_Click({
    # Clear previous status
    $lblStatus.Text = ""
    $lblStatus.ForeColor = [System.Drawing.Color]::Black

    # --- Validation ---
    if (-not $mapgenExe) {
        $lblStatus.Text = "Error: mapgen.exe not found. Build the project first."
        $lblStatus.ForeColor = [System.Drawing.Color]::Red
        return
    }

    $tileset = $txtTileset.Text.Trim()
    if ([string]::IsNullOrEmpty($tileset)) {
        $lblStatus.Text = "Error: Tileset path is required."
        $lblStatus.ForeColor = [System.Drawing.Color]::Red
        return
    }
    if (-not (Test-Path $tileset)) {
        $lblStatus.Text = "Error: Tileset path does not exist."
        $lblStatus.ForeColor = [System.Drawing.Color]::Red
        return
    }

    $output = $txtOutput.Text.Trim()
    if ([string]::IsNullOrEmpty($output)) {
        $lblStatus.Text = "Error: Output path is required."
        $lblStatus.ForeColor = [System.Drawing.Color]::Red
        return
    }

    $width = [int]$numWidth.Value
    $height = [int]$numHeight.Value

    $seed = $txtSeed.Text.Trim()

    # --- Build arguments ---
    $args = @(
        "--tileset", "`"$tileset`"",
        "--output", "`"$output`"",
        "--width", $width.ToString(),
        "--height", $height.ToString()
    )
    if (-not [string]::IsNullOrEmpty($seed)) {
        $args += @("--seed", $seed)
    }

    # --- Invoke mapgen.exe ---
    $lblStatus.Text = "Generating..."
    $lblStatus.ForeColor = [System.Drawing.Color]::Blue
    $form.Refresh()

    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $mapgenExe
        $psi.Arguments = "--tileset `"$tileset`" --output `"$output`" --width $width --height $height"
        if (-not [string]::IsNullOrEmpty($seed)) {
            $psi.Arguments += " --seed $seed"
        }
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true

        $process = [System.Diagnostics.Process]::Start($psi)
        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        $process.WaitForExit()

        $exitCode = $process.ExitCode

        if ($exitCode -eq 0) {
            $lblStatus.Text = "Success! Map saved to: $output"
            $lblStatus.ForeColor = [System.Drawing.Color]::DarkGreen
        } else {
            $errorMsg = if ($stderr) { $stderr.Trim() } else { "Unknown error (exit code $exitCode)" }
            $lblStatus.Text = "Failed (exit $exitCode): $errorMsg"
            $lblStatus.ForeColor = [System.Drawing.Color]::Red
        }
    } catch {
        $lblStatus.Text = "Error launching mapgen.exe: $_"
        $lblStatus.ForeColor = [System.Drawing.Color]::Red
    }
})
$form.Controls.Add($btnGenerate)

# Show the form
[void]$form.ShowDialog()
