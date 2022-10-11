# Setup

Create a python virtual environment:

        python -m venv <venv_dir>

Activate the virtual environment on Windows

        <venv_dir>\Scripts\activate.bat

Activate the virtual environment on Ubuntu/Linux

        source <venv_dir>/bin/activate

Install the following dependencies:

        pip install pyserial requests

# Usage

Python scripts are in the _scripts_ folder of the cloned repository.  
Ensure that board and its STLink USB is connected to PC.  
Once ready, run the following commands:

        cd scripts/comm
        python main.py

# Sample JSON Message

A sample JSON message format on how the cryto material can be exchanged from Server to
PC program is present in file _scripts/comm/sample.json_
