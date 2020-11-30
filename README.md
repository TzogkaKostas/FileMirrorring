# FileMirrorring

The purpose of this application is to monitor dynamically a hierarchy of files and directories. When there are
changes, the application updates a copy of the hierarchy in another directory. For this purpose, Linux inotify system call
interface is used.

# Compilation

make

# Execution 

./mirr source backup

The application monitors the "source" directory and when a change occurs, it updates the structure
and content of the "backup" directory.
