#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <ctime>
#include <cmath>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <map>
#include <numeric>

using namespace std;

struct cwa_timestamp {
  uint16_t year;
  uint16_t month;
  uint16_t day;
  uint16_t hours;
  uint16_t mins;
  uint16_t seconds;
};

struct OM_READER_HEADER_PACKET {
  uint16_t packetHeader;         // @ 0 +2 ASCII "MD", little-endian (0x444D)
  uint16_t packetLength;         // @ 2 +2 Packet length (1020 bytes, with header (4) = 1024 bytes total)
  uint8_t  reserved1;            // @ 4 +1 (1 byte reserved)
  uint16_t deviceId;             // @ 5 +2 Device identifier
  uint32_t sessionId;            // @ 7 +4 Unique session identifier
  uint16_t reserved2;            // @11 +2 (2 bytes reserved)
  uint32_t loggingStartTime;     // @13 +4 Start time for delayed logging
  uint32_t loggingEndTime;       // @17 +4 Stop time for delayed logging
  uint32_t loggingCapacity;      // @21 +4 Preset maximum number of samples to collect, 0 = unlimited
  uint8_t  reserved3[11];        // @25 +11 (11 bytes reserved)
  uint8_t  samplingRate;         // @36 +1 Sampling rate
  uint32_t lastChangeTime;       // @37 +4 Last change metadata time
  uint8_t  firmwareRevision;     // @41 +1 Firmware revision number
  int16_t  timeZone;             // @42 +2 Time Zone offset from UTC (in minutes), 0xffff = -1 = unknown
  uint8_t  reserved4[20];        // @44 +20 (20 bytes reserved)
  uint8_t  annotation[448];      // @64 +448 Scratch buffer / meta-data (448 characters, space padded, generally URL-encoded key/value pairs)
  uint8_t  reserved[512];        // @512 +512 Reserved for post-collection scratch buffer / meta-data (512 characters)
};

struct OM_READER_DATA_PACKET {
    uint16_t packetHeader;       // @ 0 +2  ASCII "AX", little-endian (0x5841)
    uint16_t packetLength;       // @ 2 +2  Packet length (508 bytes, with header (4) = 512 bytes total)
    uint16_t deviceFractional;   // @ 4 +2  Top bit set: 15-bit fraction of a second for the time stamp, the timestampOffset was already adjusted to minimize this assuming ideal sample rate; Top bit clear: 15-bit device identifier, 0 = unknown;
    uint32_t sessionId;          // @ 6 +4  Unique session identifier, 0 = unknown
    uint32_t sequenceId;         // @10 +4  Sequence counter, each packet has a new number (reset if restarted)
    uint32_t timestamp;          // @14 +4  Last reported RTC value, 0 = unknown
    uint16_t light;              // @18 +2  Last recorded light sensor value in raw units, 0 = none
    uint16_t temperature;        // @20 +2  Last recorded temperature sensor value in raw units, 0 = none
    uint8_t  events;             // @22 +1  Event flags since last packet, b0 = resume logging, b1 = single-tap event, b2 = double-tap event, b3-b7 = reserved for diagnostic use)
    uint8_t  battery;            // @23 +1  Last recorded battery level in raw units, 0 = unknown
    uint8_t  sampleRate;         // @24 +1  Sample rate code, (3200/(1<<(15-(rate & 0x0f)))) Hz
    uint8_t  numAxesBPS;         // @25 +1  0x32 (top nibble: number of axes = 3; bottom nibble: packing format - 2 = 3x 16-bit signed, 0 = 3x 10-bit signed + 2-bit exponent)
    int16_t  timestampOffset;    // @26 +2  Relative sample index from the start of the buffer where the whole-second timestamp is valid
    uint16_t sampleCount;        // @28 +2  Number of accelerometer samples (80 or 120)
    uint8_t  rawSampleData[480]; // @30 +480 Raw sample data.  Each sample is either 3x 16-bit signed values (x, y, z) or one 32-bit packed value (The bits in bytes [3][2][1][0]: eezzzzzz zzzzyyyy yyyyyyxx xxxxxxxx, e = binary exponent, lsb on right)
    uint16_t checksum;           // @510 +2 Checksum of packet (16-bit word-wise sum of the whole packet, should be zero)
};

template <typename T>
float mean(vector<T> vec) {
  float sum = accumulate(begin(vec), end(vec), 0.0);
  float mean =  sum / vec.size();

  return mean;
}

template <typename T>
float stdDev(vector<T> vec) {
  float sum = accumulate(begin(vec), end(vec), 0.0);
  float mean =  sum / vec.size();

  float accum = 0.0;
  for (int i = 0; i < vec.size(); ++i) {
    accum += pow((float) vec[i] - mean, 2);
  }

  float stdev = sqrt(accum / (vec.size()-1));

  return stdev;
}

template <typename T>
void summarise_individual(string biobank_id, vector<cwa_timestamp> &cwa_timestamps,
                          vector<T> &temperatures, string summary_filename) {
  float total_mean = 0.0;
  float sd = 0.0;
  //float median = 0.0;
  map<int, vector<double>> hourly_temps;

  total_mean = mean(temperatures);
  sd = stdDev(temperatures);
  //median = temperatures[(int) temperatures.size()/2];

  for (int i = 0; i < temperatures.size(); ++i) {
    cwa_timestamp ts = cwa_timestamps[i];
    int hour = ts.hours;
    hourly_temps[hour].push_back(temperatures[i]);
  }

  ofstream summary_file;
  summary_file.open(summary_filename, fstream::app);

  summary_file << biobank_id << "," << to_string(total_mean) << "," << to_string(sd) << ",";

  for (int i = 0; i < 24; ++i) {
    try {
      float temp_mean = mean(hourly_temps[i]);
      float temp_sd = stdDev(hourly_temps[i]);
      //float temp_med = hourly_temps[i][(int) hourly_temps[i].size()/2];

      summary_file << temp_mean << ",";
      summary_file << temp_sd;
      //summary_file << temp_med;
      if (i == 23) {
        summary_file << "\n";
      } else {
        summary_file << ",";
      }
    } catch (...) {
      cerr << "Error occured" << endl;
    }
  }

  summary_file.close();
}


void process_header(char header_buffer[], OM_READER_HEADER_PACKET &header) {
  memcpy(&header.packetHeader,     header_buffer,       2);
  memcpy(&header.packetLength,     header_buffer + 2,   2);
  memcpy(&header.reserved1,        header_buffer + 4,   1);
  memcpy(&header.deviceId,         header_buffer + 5,   2);
  memcpy(&header.sessionId,        header_buffer + 7,   4);
  memcpy(&header.reserved2,        header_buffer + 11,  2);
  memcpy(&header.loggingStartTime, header_buffer + 13,  4);
  memcpy(&header.loggingEndTime,   header_buffer + 17,  4);
  memcpy(&header.loggingCapacity,  header_buffer + 21,  4);
  memcpy(&header.reserved3,        header_buffer + 25,  11);
  memcpy(&header.samplingRate,     header_buffer + 36,  1);
  memcpy(&header.lastChangeTime,   header_buffer + 37,  4);
  memcpy(&header.firmwareRevision, header_buffer + 41,  1);
  memcpy(&header.timeZone,         header_buffer + 42,  2);
  memcpy(&header.reserved4,        header_buffer + 44,  20);
  memcpy(&header.annotation,       header_buffer + 64,  448);
  memcpy(&header.reserved,         header_buffer + 512, 512);
}

void process_data_block(char data_buffer[], OM_READER_DATA_PACKET &data) {
  memcpy(&data.packetHeader,       data_buffer,       2);
  memcpy(&data.packetLength,       data_buffer + 2,   2);
  memcpy(&data.deviceFractional,   data_buffer + 4,   2);
  memcpy(&data.sessionId,          data_buffer + 6,   4);
  memcpy(&data.sequenceId,         data_buffer + 10,  4);
  memcpy(&data.timestamp,          data_buffer + 14,  4);
  memcpy(&data.light,              data_buffer + 18,  2);
  memcpy(&data.temperature,        data_buffer + 20,  2);
  memcpy(&data.events,             data_buffer + 22,  1);
  memcpy(&data.battery,            data_buffer + 23,  1);
  memcpy(&data.sampleRate,         data_buffer + 24,  1);
  memcpy(&data.numAxesBPS,         data_buffer + 25,  1);
  memcpy(&data.timestampOffset,    data_buffer + 26,  2);
  memcpy(&data.sampleCount,        data_buffer + 28,  2);
  memcpy(&data.rawSampleData,      data_buffer + 30,  480);
  memcpy(&data.checksum,           data_buffer + 510, 2);
}

void process_cwa_timestamp(uint32_t raw, cwa_timestamp &timestamp) {
  timestamp.year  = ((raw >> 26) & 0x3f) + 2000;
  timestamp.month = ((raw >> 22) & 0x0f);
  timestamp.day   = ((raw >> 17) & 0x1f);
  timestamp.hours = ((raw >> 12) & 0x1f);
  timestamp.mins  = ((raw >> 6)  & 0x3f);
  timestamp.seconds  =   raw     & 0x3f;
}

void show_header_details(OM_READER_HEADER_PACKET &header) {
  cwa_timestamp ts_start;
  cwa_timestamp ts_end;
  cwa_timestamp ts_change;
  process_cwa_timestamp(header.loggingStartTime, ts_start);
  process_cwa_timestamp(header.loggingEndTime, ts_end);
  process_cwa_timestamp(header.lastChangeTime, ts_change);

  cout << "------------Header Details------------" << endl;
  cout << setw(20) << left << "Packet Header: " << (char*) &header.packetHeader << endl;
  cout << setw(20) << "Packet Length: " << header.packetLength << endl;
  cout << setw(20) << "Device ID: " << header.deviceId << endl;
  cout << setw(20) << "Session ID: " << header.sessionId << endl;
  cout << setw(20) << "Logging Start Time: " << ts_start.day << " " << ts_start.month << " " << ts_start.year << " " << ts_start.hours << ":" << ts_start.mins << ":" << ts_start.seconds << endl;
  cout << setw(20) << "Logging End Time: " << ts_end.day << " " << ts_end.month << " " << ts_end.year << " " << ts_end.hours << ":" << ts_end.mins << ":" << ts_end.seconds << endl;
  cout << setw(20) << "Logging Capacity: " << header.loggingCapacity << " (0 = unlimited)" << endl;
  cout << setw(20) << "Sampling Rate: " << (int) header.samplingRate << " Hz" << endl;
  cout << setw(20) << "Last Change Time: " << ts_change.day << " " << ts_change.month << " " << ts_change.year << " " << ts_change.hours << ":" << ts_change.mins << ":" << ts_change.seconds << endl;
  cout << setw(20) << "Firmware Revision: " << (int) header.firmwareRevision << endl;
  cout << setw(20) << "Time Zone: " << header.timeZone << " (-1 = unknown)" << endl;

}


/*
 * This and read_light_data are essentially the same, should refactor them together in to a more generic function
 */
void read_temp_data(ifstream &file, vector<uint16_t> &temps, vector<string> &timestamps) {
  // Read data blocks until nothing is left
  while (file.peek() != EOF) {
    char data_buffer[512];
    file.read(data_buffer, 512);

    // Could use this to be more generic and to extract more data in future
    // OM_READER_DATA_PACKET data;
    // process_data_block();

    int16_t temp;
    uint32_t raw_timestamp;
    cwa_timestamp timestamp;

    memcpy(&temp, data_buffer + 20, 2);      // Extract Temperature from data block
    memcpy(&raw_timestamp, data_buffer + 14, 4); // Extract timestamp from data block
    temp = (temp * 150.0 - 20500) / 1000.0; // Convert to degrees C
    temps.push_back(temp);

    process_cwa_timestamp(raw_timestamp, timestamp);
    char timestamp_string[19];
    sprintf(timestamp_string, "%4d-%02d-%02d %02d:%02d:%02d", timestamp.year,
                                                         timestamp.month,
                                                         timestamp.day,
                                                         timestamp.hours,
                                                         timestamp.mins,
                                                         timestamp.seconds);
    timestamps.push_back(timestamp_string);
  }
}

void read_light_data(ifstream &file, vector<uint16_t> &light, vector<string> &timestamps) {
  while (file.peek() != EOF) {
    char data_buffer[512];
    file.read(data_buffer, 512);

    uint16_t lux;
    uint32_t raw_timestamp;
    cwa_timestamp timestamp;

    memcpy(&lux, data_buffer + 18, 2);  // Extract Light value from data block
    memcpy(&raw_timestamp, data_buffer + 14, 4); // Extract timestamp
    // No point trying to convert to lux (for BiobankUK Data), light sensor was partially covered 
    //double log10LuxTimes10Power3 = ((lux + 512.0) * 6000 / 1024);
    //lux = pow(10.0, log10LuxTimes10Power3 / 1000.0); // In lux, supposedly
    light.push_back(lux);

    process_cwa_timestamp(raw_timestamp, timestamp);
    char timestamp_string[19];
    sprintf(timestamp_string, "%04d-%02d-%02d %02d:%02d:%02d", timestamp.year,
                                                         timestamp.month,
                                                         timestamp.day,
                                                         timestamp.hours,
                                                         timestamp.mins,
                                                         timestamp.seconds);

    timestamps.push_back(timestamp_string);
  }
}

template <typename T>
void save_temps_to_file(vector<T> &temps, vector<string> &timestamps, string filename) {
  uint16_t difference = 0;
  if (temps.size() != timestamps.size()) {
    // Temperatures have been averaged
    difference = timestamps.size() - temps.size();
  }

  // Save temperatures to file
  ofstream temp_file;
  temp_file.open(filename, ios::out);
  temp_file << "timestamp, temp\n";
  for (int i = difference; i < temps.size(); ++i) {
    temp_file << timestamps[i] << ", " << temps[i] << "\n";
  }

  temp_file.close();
}

template <typename T>
void save_light_to_file(vector<T> &light, vector<string> &timestamps, string filename) {
  uint16_t difference = 0;
  if (light.size() != timestamps.size()) {
    // Temperatures have been averaged
    difference = timestamps.size() - light.size();
  }

  // Save temperatures to file
  ofstream light_file;
  light_file.open(filename, ios::out);
  light_file << "timestamp, light\n";
  for (int i = difference; i < light.size(); ++i) {
    light_file << timestamps[i] << ", " << light[i] << "\n";
  }

  light_file.close();
}

template <typename T>
void save_temps_and_sigs(vector<T> &temps, vector<int> &signals, string filename) {
  ofstream temp_file;
  temp_file.open(filename, ios::out);

  for (int i = 0; i < temps.size(); ++i) {
    temp_file << i << " " << temps[i] << " " << signals[i] << "\n";
  }

  temp_file.close();
}

void central_moving_average(vector<uint16_t> &temps, vector<float> &av_temps, uint16_t window_size) {
  uint32_t n = temps.size();
  if (n <= window_size) {
    cerr << "Window size too large (" << window_size << ") to compute central MA for data of length " << n << endl;
    exit(1);
  }

  uint32_t start, end;
  start = window_size / 2;
  end = n - (window_size / 2);

  for (int i = start; i < end; ++i) {
    float sum = 0.0;
    int evals = 0;

    // Left of central point
    for (int l = i - (window_size/2); l < i; ++l) {
      sum += (float) temps[l];
    }

    // Right of central point
    for (int r = i; r < i + (window_size/2); ++r) {
      sum += (float) temps[r];
    }

    av_temps.push_back(sum/window_size);
  }

}

// NOT USED, kept for potential future use
//
// Based on "Smoothed z-score algo" found on SO, here:
// https://stackoverflow.com/questions/22583391/peak-signal-detection-in-realtime-timeseries-data
// Needs optimizing
void peak_detect(vector<uint16_t> &temps, vector<int> &sigs,
                 uint16_t lag, float threshold, float influence) {

  for (int i = 0; i < temps.size(); ++i) {
    sigs.push_back(0);
  }

  vector<float> filteredInput(temps.size(), 0.0);
  vector<float> avgFilter(temps.size(), 0.0);
  vector<float> stdFilter(temps.size(), 0.0);
  vector<float> subVecStart(temps.begin(), temps.begin() + lag);

  avgFilter[lag] = mean(subVecStart);
  stdFilter[lag] = stdDev(subVecStart);

  for (int i = lag + 1; i < temps.size(); ++i) {
    if (abs(temps[i] - avgFilter[i-1]) > threshold * stdFilter[i-1]) {
      if (temps[i] > avgFilter[i-1]) {
        sigs[i] = 1;
      } else {
        // If we wanted negative signals for troughs too
        sigs[i] = -1;
      }
      // Makes the influence lower
      filteredInput[i] = influence*temps[i] + (1 - influence) * filteredInput[i-1];
    } else {
      sigs[i] = 0;
      filteredInput[i] = temps[i];
    }

    //Adjust the filters
    vector<float> subVec(filteredInput.begin() + i - lag, filteredInput.begin() + i);
    avgFilter[i] = mean(subVec);
    stdFilter[i] = stdDev(subVec);
  }
}

void print_usage() {
  cout << "----------Axivity AX3 Temperature Extractor----------" << endl;
  cout << "Author: James Christie (jlc_christie@live.com)" << endl;
  cout << "Date: June 2018" << endl;
  cout << "Usage: ./ax3 -t|-l -i [input file] -o [output file]" << endl;
  cout << "\t-t -- Temperature Mode" << endl;
  cout << "\t-l -- Light Mode" << endl;
  cout << "\t-i -- Path to .cwa input file" << endl;
  cout << "\t-o -- Path to output file to be generated" << endl;
  cout << "\t-s -- Path to summary statistics file (appends if exists)" << endl;
  cout << "\t-a -- Use a central moving average on the read data (Window Size" << endl;
  cout << "\t      must be passed in to args after -a, e.g. -a 50" << endl;
}

int main(int argc, char* argv[]) {
  string in_filename;
  string out_filename;
  string sum_filename;
  string biobank_id;
  bool temp_mode = false;
  bool mode_set = false;
  bool in_set = false;
  bool out_set = false;
  bool use_sma = false;
  uint16_t win_size = 50;
  

  for (int i = 1; i < argc; ++i) {
    switch (string(argv[i])[1]) {
      case 'l':
        if (mode_set) {
	  cerr << "error: cannot use -t and -l flags together" << endl;
	  exit(1);
        }
        mode_set = true;
	break;
      case 't':
        if (mode_set) {
	  cerr << "error: cannot use -t and -l flags together" << endl;
	  exit(1);
	}
	temp_mode = true;
	mode_set = true;
	break;
      case 'i':
	in_filename = string(argv[i+1]);
	in_set = true;
	biobank_id = in_filename.substr(0, in_filename.find("_"));
	++i;
	break;
      case 'o':
        out_filename = string(argv[i+1]);
	out_set = true;
	++i;
	break;
      case 's':
	//sum_filename = string(argv[i+1]).substr(string(argv[i+1]).end()-3, string(argv[i+1]).end());
	++i;
	break;
      case 'a':
	use_sma = true;
	win_size = stoi(string(argv[i+1]));
	++i;
	break;
      default:
	cerr << "error: argument not recognised: " << string(argv[i]) << endl;
	cerr << "see usage: \n" << endl;
	print_usage();
	exit(1);
    }
  }

  if (!mode_set) {
    cerr << "error: mode not set, use -t for temperature, -l for light" << endl;
    exit(1);
  } else if (!in_set) {
    cerr << "error: no input file given" << endl;
    exit(1);
  } else if (!out_set) {
    cerr << "error: no output file given" << endl;
    exit(1);
  }

  ifstream aws_file (in_filename, ios::in | ios::binary);

  // Read header
  char header_buffer[1024];
  if (!aws_file.read(header_buffer, 1024)) {
    cerr << "Error occured reading header of file, exiting." << endl;
    exit(1);
  }

  OM_READER_HEADER_PACKET header;
  process_header(header_buffer, header);
  //show_header_details(header);

  vector<uint16_t> temperatures;
  vector<uint16_t> light;
  vector<string> timestamps;
  vector<cwa_timestamp> cwa_timestamps;

  if (temp_mode) {
    read_temp_data(aws_file, temperatures, timestamps);
  } else {
    read_light_data(aws_file, light, timestamps);
  }


  vector<float> av;
  if (use_sma) {
    if (temp_mode) {
      central_moving_average(temperatures, av, win_size);
      save_temps_to_file(av, timestamps, out_filename);
    } else {
      central_moving_average(light, av, win_size);
      save_light_to_file(av, timestamps, out_filename);
    }

  } else {
    if (temp_mode) {
      save_temps_to_file(temperatures, timestamps, out_filename);
    } else {
      save_light_to_file(light, timestamps, out_filename);
    }
  }

  string sum = "/gpfs/mrc0/projects/Research_Project-MRC158833/jlcc201/sumstats/" + sum_filename;
  //summarise_individual(biobank_id, cwa_timestamps, temperatures, sum);

  aws_file.close();

  return 0;
}
