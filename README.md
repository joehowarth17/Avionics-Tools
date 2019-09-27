# Avionics PC Software Tools
This repository contains various programs that are used with the UMSATS student designed flight computer.

## Versions
#### v1.0.0 - 2019 Spaceport America Cup Software Tools

## UART Serial Reader Utility
This is a python program that reads from a serial COM port, and logs the raw bytes to a log file.
This should be used when downloading flight data from the flight computer.
More information can be found in the README inside the folder.

## Data Parser Utility
This is a C program that will turn a log file into a csv file. 
This is needed because the data is stored on the flight computer as custom format variable length packets.
More information can be found in the README inside the folder.

