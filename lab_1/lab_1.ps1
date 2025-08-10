param(
    [string]$path,
    [string]$last_data
)


# получаем дату
$date = [DateTime]::ParseExact($last_data, "yyyy-MM-dd", $null)

# Проверка на существование пути
if ( -not (Test-Path -Path $path)) {
    Write-Host "Путь $path не существует."
} else {
# получаем необходимые файлы
$sorted_files = Get-ChildItem -Path $path -Filter "*.7z" -Recurse | Sort-Object -Property Length -Descending
}

foreach ($file in $sorted_files) {
    $newName = "Бекап_" + $date.ToString("yyyy-MM-dd") + ".7z" # формируем название
    Rename-Item -Path $file.FullName -NewName $newName # переименовываем
    $date = $date.AddDays(-7) # меняем дату
}