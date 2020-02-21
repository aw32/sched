# test_task

`test_task` allows to test task behavior separately from the scheduler for singular cases.
For this `test_task` reads commands from a text file and replicates scheduler communication.

There are 3 types of commands with one command per line:

* WAIT: `wait 1.5`, wait for 1.5 seconds
* SEND: `send {"msg":"TASK_SUSPEND","id":0}`, the message (including a trailing null-byte) is being sent to the client
* RECV: `recv {"msg":"TASK_STARTED","id":0}`, (null-byte separated) messages are being read from the client until the given message appears, until then this command blocks
* lines starting with "#" are being ignored

Afterwards the tool shuts down.

## Running

`./test_task messages.txt`

The socket opened is `/tmp/test.socket`.
In case the file already exist, remove it and restart.

After starting the tool, start the task using the correct socket path.

## test_task.py

The python script does the same thing, but slower.
