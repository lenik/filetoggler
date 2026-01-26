# Bash completion for ft (filetoggler alias)
# Source the main filetoggler completion
if [[ -f /usr/share/bash-completion/completions/filetoggler ]]; then
    source /usr/share/bash-completion/completions/filetoggler
    complete -F _filetoggler ft
fi
