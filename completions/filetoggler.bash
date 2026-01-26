_filetoggler() {
    local IFS=$'\n'
    local cword=${COMP_CWORD}
    COMPREPLY=( $(filetoggler --complete-bash "$cword" "${COMP_WORDS[@]}") )
    return 0
}

complete -F _filetoggler filetoggler
