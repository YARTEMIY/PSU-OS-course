param(
    [string]$path,
    [string]$last_data
)


# �������� ����
$date = [DateTime]::ParseExact($last_data, "yyyy-MM-dd", $null)

# �������� �� ������������� ����
if ( -not (Test-Path -Path $path)) {
    Write-Host "���� $path �� ����������."
} else {
# �������� ����������� �����
$sorted_files = Get-ChildItem -Path $path -Filter "*.7z" -Recurse | Sort-Object -Property Length -Descending
}

foreach ($file in $sorted_files) {
    $newName = "�����_" + $date.ToString("yyyy-MM-dd") + ".7z" # ��������� ��������
    Rename-Item -Path $file.FullName -NewName $newName # ���������������
    $date = $date.AddDays(-7) # ������ ����
}