# Axivity AX3 Light and Temperature Extractor

A tool created to extract the raw light and temperature data from Axivity AX3 wrist-worn accelerometers (as used in the UK Biobank). This was created whilst working at the Complex Trait Genomics team at the University of Exeter Medical School (http://www.t2diabetesgenes.org/who-we-are/). 

Features:
- convert large .cwa binary files to .csv 
- light data extraction
- temperature data extraction 
- central moving average of data (variable window size)
- summarise individuals light or temperature data (useful for summarising many individuals in to one file)
- framework exists to extract other data such as raw accelerometer readings 

## Install
1. clone this repo \
   `git clone https://github.com/jlc-christie/axivity-ax3-tool.git` 
2. compile with prefered c++11 compatible compiler \
   e.g. `g++ ax3.cpp -o ax3 -O3`, if using g++

## Example Usage
Extract the **`light (-l)`** data from the input file **`my_raw_data_file.cwa`**, use a central moving average **(`-a`)** of 5 minutes (300 seconds) and save the output to **`light_data.csv`**. Also, append summary statistics about the light data to the file **`summary_statistics.csv`**.
```
./ax3 -l -i my_raw_data_file.cwa -o light_data.csv -a 300 -s summary_statistics.csv
```

## Plotting 
This repo also contains a small gnuplot script to either interactively plot the results or save the plots to files. Use `-e "filename='out.csv'"` to pass in the data file, making sure that the -e flag comes **before** the script file. Append `-p` to display the interactive plot and finally pass another argument `outfile` if you want the plots to be saved to a file (recommended, as interactive plot is relatively slow). 
Examples:
1. Display interactive plot, don't save plot images \
   `gnuplot -e "filename='out.csv'" plot.gpl -p`
2. Display interactive plot **and** save plot images \
   `gnuplot -e "filename='out.csv';outfile='my_plot'" plot.gpl -p` \
   *creates 2 files, `my_plot.png` and `my_plot.ps`, ps can be converted easily to pdf if required (e.g. `ps2pdf my_plot.ps my_plot.pdf`)*
3. Don't display interactive plot, just save images to files \
   `gnuplot -e "filename='out.csv';outfile='my_plot'" plot.gpl`
   
## Summary Statistics
As mentioned above, the `-s` flag followed by a summary file filename, will calculate the mean and standard deviation of light or temperature (depending on which mode it is in) for the whole day as well as hourly. Because this functionality is meant to be used as part of a batch script which appends to the same file, there is **no header**. The data is comma seperated and the format is as follows:

| filename | mean | iqr | std_dev | hour_0_mean | hour_0_std_dev | hour_1_mean | hour_1_std_dev | ... | hour_23_mean | hour_23_std_dev
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 27492749 | 20.558 | 120.000 |1.381 | 21.550 | 1.656 | 21.229 | 1.524 | ... | 20.972 | 1.120 |


