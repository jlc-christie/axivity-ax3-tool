# Axivity AX3 Light and Temperature Extractor

A tool created to extract the raw light and temperature data from Axivity AX3 wrist-worn accelerometers (as used in the UK Biobank). 

Features:
- convert large .cwa binary files to .csv 
- light data extraction
- temperature data extraction 
- central moving average of data (variable window size)
- summarise individuals light or temperature data (useful for summarising many individuals in to one file)
- framework exists to extract other data such as raw accelerometer readings 

## Install
1. clone this repo (git clone https://github.com/jlc-christie/axivity-ax3-tool.git) 
2. compile with prefered c++11 compatible compiler (g++ ax3.cpp -o ax3 -O3)

## Example Usage
Extract the **`light (-l)`** data from the input file **`my_raw_data_file.cwa`**, use a central moving average **(`-a`)** of 5 minutes (300 seconds) and save the output to **`light_data.csv`**. Also, append summary statistics about the light data to the file **`summary_statistics.csv`**.
```
./ax3 -l -i my_raw_data_file.cwa -o light_data.csv -a 300 -s summary_statistics.csv
```
