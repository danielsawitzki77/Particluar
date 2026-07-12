# MapGenerator GUI — Windows Forms frontend for the MapGenerator CLI tool
# Provides a visual interface for offline map generation.

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = "Particluar - Map Generator"
$form.Size = New-Object System.Drawing.Size(460, 340)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false

# Tileset folder
$lblTileset = New-Object System.Windows.Forms.Label
$lblTileset.Location = New-Object System.Drawing.Point(10, 20)
$lblTileset.Size = New-Object System.Drawing.Size(80, 20)
$lblTileset.Text = "Tileset folder:"
$form.Controls.Add($lblTileset)

$txtTileset = New-Object System.Windows.Forms.TextBox
$txtTileset.Location = New-Object System.Drawing.Point(100, 18)
$txtTileset.Size = New-Object System.Drawing.Size(260, 20)
$form.Controls.Add($txtTileset)

$btnBrowseTileset = New-Object System.Windows.Forms.Button
$btnBrowseTileset.Location = New-Object System.Drawing.Point(370, 16)
$btnBrowseTileset.Size = New-Object System.Drawing.Size(60, 24)
$btnBrowseTileset.Text = "Browse"
$btnBrowseTileset.Add_Click({
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = "Select tileset folder (contains .json + .png)"
    if ($dlg.ShowDialog() -eq "OK") { $txtTileset.Text = $dlg.SelectedPath }
})
$form.Controls.Add($btnBrowseTileset)

# Output file
$lblOutput = New-Object System.Windows.Forms.Label
$lblOutput.Location = New-Object System.Drawing.Point(10, 55)
$lblOutput.Size = New-Object System.Drawing.Size(80, 20)
$lblOutput.Text = "Output file:"
$form.Controls.Add($lblOutput)

$txtOutput = New-Object System.Windows.Forms.TextBox
$txtOutput.Location = New-Object System.Drawing.Point(100, 53)
$txtOutput.Size = New-Object System.Drawing.Size(260, 20)
$txtOutput.Text = "output_map.json"
$form.Controls.Add($txtOutput)

$btnBrowseOutput = New-Object System.Windows.Forms.Button
$btnBrowseOutput.Location = New-Object System.Drawing.Point(370, 51)
$btnBrowseOutput.Size = New-Object System.Drawing.Size(60, 24)
$btnBrowseOutput.Text = "Browse"
$btnBrowseOutput.Add_Click({
    $dlg = New-Object System.Windows.Forms.SaveFileDialog
    $dlg.Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*"
    $dlg.DefaultExt = "json"
    if ($dlg.ShowDialog() -eq "OK") { $txtOutput.Text = $dlg.FileName }
})
$form.Controls.Add($btnBrowseOutput)

# Width
$lblWidth = New-Object System.Windows.Forms.Label
$lblWidth.Location = New-Object System.Drawing.Point(10, 95)
$lblWidth.Size = New-Object System.Drawing.Size(80, 20)
$lblWidth.Text = "Width (cells):"
$form.Controls.Add($lblWidth)

$nudWidth = New-Object System.Windows.Forms.NumericUpDown
$nudWidth.Location = New-Object System.Drawing.Point(100, 93)
$nudWidth.Size = New-Object System.Drawing.Size(80, 20)
$nudWidth.Minimum = 1
$nudWidth.Maximum = 4096
$nudWidth.Value = 64
$form.Controls.Add($nudWidth)

# Height
$lblHeight = New-Object System.Windows.Forms.Label
$lblHeight.Location = New-Object System.Drawing.Point(200, 95)
$lblHeight.Size = New-Object System.Drawing.Size(80, 20)
$lblHeight.Text = "Height (cells):"
$form.Controls.Add($lblHeight)

$nudHeight = New-Object System.Windows.Forms.NumericUpDown
$nudHeight.Location = New-Object System.Drawing.Point(290, 93)
$nudHeight.Size = New-Object System.Drawing.Size(80, 20)
$nudHeight.Minimum = 1
$nudHeight.Maximum = 4096
$nudHeight.Value = 64
$form.Controls.Add($nudHeight)

# Seed
$lblSeed = New-Object System.Windows.Forms.Label
$lblSeed.Location = New-Object System.Drawing.Point(10, 130)
$lblSeed.Size = New-Object System.Drawing.Size(80, 20)
$lblSeed.Text = "Seed (0=rand):"
$form.Controls.Add($lblSeed)

$nudSeed = New-Object System.Windows.Forms.NumericUpDown
$nudSeed.Location = New-Object System.Drawing.Point(100, 128)
$nudSeed.Size = New-Object System.Drawing.Size(120, 20)
$nudSeed.Minimum = 0
$nudSeed.Maximum = 999999999
$nudSeed.Value = 0
$form.Controls.Add($nudSeed)

# Generate button
$btnGenerate = New-Object System.Windows.Forms.Button
$btnGenerate.Location = New-Object System.Drawing.Point(100, 175)
$btnGenerate.Size = New-Object System.Drawing.Size(120, 35)
$btnGenerate.Text = "Generate Map"
$btnGenerate.Add_Click({
    $tileset = $txtTileset.Text.Trim()
    $output = $txtOutput.Text.Trim()
    $w = [int]$nudWidth.Value
    $h = [int]$nudHeight.Value
    $seed = [int]$nudSeed.Value

    if (-not $tileset) {
        [System.Windows.Forms.MessageBox]::Show("Please select a tileset folder.", "Error")
        return
    }
    if (-not $output) {
        [System.Windows.Forms.MessageBox]::Show("Please specify an output file.", "Error")
        return
    }

    $exe = Join-Path $PSScriptRoot "..\bin\Debug\MapGenerator.exe"
    if (-not (Test-Path $exe)) {
        $exe = Join-Path $PSScriptRoot "..\bin\Release\MapGenerator.exe"
    }
    if (-not (Test-Path $exe)) {
        [System.Windows.Forms.MessageBox]::Show("MapGenerator.exe not found. Build the solution first.", "Error")
        return
    }

    $args = "--tileset `"$tileset`" --output `"$output`" --width $w --height $h"
    if ($seed -gt 0) { $args += " --seed $seed" }

    $lblStatus.Text = "Generating..."
    $form.Refresh()

    $proc = Start-Process -FilePath $exe -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\mapgen_out.txt" -RedirectStandardError "$env:TEMP\mapgen_err.txt"

    if ($proc.ExitCode -eq 0) {
        $out = Get-Content "$env:TEMP\mapgen_out.txt" -Raw
        $lblStatus.Text = $out.Trim()
    } else {
        $err = Get-Content "$env:TEMP\mapgen_err.txt" -Raw
        $lblStatus.Text = "FAILED"
        [System.Windows.Forms.MessageBox]::Show($err, "Generation Failed")
    }
})
$form.Controls.Add($btnGenerate)

# Status
$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.Location = New-Object System.Drawing.Point(10, 230)
$lblStatus.Size = New-Object System.Drawing.Size(420, 50)
$lblStatus.Text = "Ready."
$form.Controls.Add($lblStatus)

$form.ShowDialog() | Out-Null
