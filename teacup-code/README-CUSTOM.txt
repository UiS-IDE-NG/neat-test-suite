Custom traffic:

Custom traffic is used with the "start_custom_traffic" task.

Example:

traffic_custom = [
    ('0.0', '1', " start_custom_traffic,"
     " name='command',"
     " directory='directory/where/command/can/be/found',"
     " hostname='testhost1',"
     " copy_file='0',"
     " add_prefix='0',"
     " parameters='-l %s -M %s -p %s -v %s -u %s' % (V_data_size, V_transports, 8080, 0, 2)"),
]

Custom traffic supports the following options:

name:
The name of the command/program to run.
NOTE: If "directory" option is not specified, it must be available from
one of the directories specified in the PATH environment variable of the remote host (specified
by "hostname" option).

directory:
The directory in which the program specified by "name" can be found.
NOTE 1: If "copy_file" option is enabled, "directory" specifies the directory on the control node
where the program can be found. The program will be copied
to /usr/bin on the testhost specified by "hostname".
NOTE 2: If "copy_file" option is not enabled,
"directory" simply means the directory on the remote host specified by "hostname" where
the program can be found.

hostname:
The name of the testhost to run the program on.

copy_file:
If enabled ('1'), the program/command will be copied to the remote host before being
run. This can allow the user to gather all scripts/commands on the control node.
If disabled ('0'), the program will need to exist on the remote host.

add_prefix:
If enabled ('1'), adds the current TEACUP experiment prefix name as the last argument
to the command specified by "name". This can allow the command to produce logs with filenames
similar to those produced as results by TEACUP, or can allow the program to know the current
experiment configurations.

parameters:
The parameters that will be added to the program name. TEACUP V_ variables are supported.


Custom loggers:

Custom loggers can be specified through the "TPCONF_custom_loggers" config variable.
The task "start_custom_logger" will need to be specified.

Example:

TPCONF_custom_loggers = [
    ('6', " start_custom_logger,"
     " name='my_cool_logger.sh',"
     " logname='cooldata',"
     " directory='/path/to/directory',"
     " copy_file='0',"
     " add_prefix='0',"
     " hostname='testhost1',"
     " parameters='100 100 10 100'"),
]

Custom loggers supports the same options as custom traffic with the inclusion of "logname".

logname:
The name that will be appended to the TEACUP experiment prefix string. This name will
identify the logger.
