_forge_completion() {
    local cur prev opts commands
    COMPREPLY=() # Array for completion suggestions
    cur="${COMP_WORDS[COMP_CWORD]}" # Current word being typed
    prev="${COMP_WORDS[COMP_CWORD-1]}" # Previous word
    opts=$(forge OPTIONS)
    commands=$(forge COMMANDS)

    # Function to extract package names from 'forge list'
    _get_package_names() {
        if command -v forge >/dev/null 2>&1; then
            # Extract package names from the 'Name' column, skipping header
            forge list 2>/dev/null | awk 'NR>2 {print $1}' | grep -v '^----'
        fi
    }

    # Function to extract repository names from 'forge list-repos'
    _get_repo_names() {
        if command -v forge >/dev/null 2>&1; then
            # Extract first column (repository names), strip color codes, skip header
            forge list-repos 2>/dev/null | sed -r 's/\x1B\[[0-9;]*[mK]//g' | awk 'NR>2 && $1 !~ /^----/ {print $1}'
        fi
    }

    # Handle completion based on context
    case "${prev}" in
        forge)
            # Suggest commands and options after 'forge'
            COMPREPLY=( $(compgen -W "${commands} ${opts}" -- "${cur}") )
            return 0
            ;;
        -h|--help)
            # Suggest flags, commands, or '*' for --help=<flag|cmd>
            COMPREPLY=( $(compgen -W "${opts} ${commands} *" -- "${cur}") )
            return 0
            ;;
        search|install|uninstall|update|save-dep|deps|new|edit|dump|drop|files|restore)
            # Suggest package names for package-related commands
            COMPREPLY=( $(compgen -W "$(_get_package_names)" -- "${cur}") )
            return 0
            ;;
        drop-repo)
            # Suggest repository names from 'forge list-repos'
            COMPREPLY=( $(compgen -W "$(_get_repo_names)" -- "${cur}") )
            return 0
            ;;
        add-repo|create-repo)
            # Allow free typing for git links or names
            return 0
            ;;
        api)
            # Suggest package names or nothing if no argument
            COMPREPLY=( $(compgen -W "$(_get_package_names)" -- "${cur}") )
            return 0
            ;;
        *)
            # Default: suggest commands and options
            COMPREPLY=( $(compgen -W "${commands} ${opts}" -- "${cur}") )
            return 0
            ;;
    esac
}

# Register the completion function for 'forge'
complete -F _forge_completion forge
