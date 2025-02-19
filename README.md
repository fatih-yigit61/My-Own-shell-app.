Medsh - Terminal Shell

Medsh is a user-friendly terminal shell application. It offers user history management, alias definition, and multi-process execution support. Developed in C, it is designed to run on UNIX/Linux systems.

Features

User Management: Maintains command history per user.

History Navigation: Users can access previous commands using the up/down arrow keys.

Alias Support: Commands can be assigned aliases for easier reuse.

Background Processes: Commands can be executed in the background using the '&' symbol.

Profile Storage: User aliases are stored in the .profile_medsh file.

Signal Handling: Child processes are managed properly to prevent zombie processes.



Usage

Execute a command:

ls -l

Run a command in the background:

sleep 10 &

Create an alias:

alias definition ll:"ls -la"

View command history:

history

Navigate previous commands:

↑ (Up arrow key): Move to the previous command.

↓ (Down arrow key): Move to the next command.



To contribute to this project, you can fork it and submit a pull request.
