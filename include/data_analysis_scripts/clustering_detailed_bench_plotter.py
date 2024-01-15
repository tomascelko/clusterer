import numpy as np
import plotly
import plotly.graph_objects as go

NO_MARKER_STYLE = dict(
            #color='LightSkyBlue',
            size=20,
            line = dict()
)

PLOT_COLORS = ["blue", "red", "green", "black"]

class parser:
  PIPE_NAME_SEPARATOR = "---->"
  PIPE_OCCUPANCY_SEPARATOR = ":"
  HITRATE_SEPARATOR = " "
  HITRATE_IDENTIFIER = "hitrate"
  def __init__(self):
    pass
  def is_pipe_occupancy_line(self, line):
    return self.PIPE_NAME_SEPARATOR in line
  
  def is_hitrate_line(self, line):
    return self.HITRATE_IDENTIFIER in line
  
  def parse_pipe_name(self, line):
    line_tokens = line.split(self.PIPE_OCCUPANCY_SEPARATOR)
    return line_tokens[0]
  
  def parse_occupancy(self, line):
    line_tokens = line.split(self.PIPE_OCCUPANCY_SEPARATOR)
    return int(line_tokens[2])
  
  def parse_hitrate(self, line):
    line_tokens = line.split(":")
    value_tokens = line_tokens[1].split(" ")
    return {line_tokens[0] : float(value_tokens[1])}
    
  def parse_pipe_occupancies(self, filename):
    pipe_occupancies = {}
    with open(filename, 'r') as log_file:
      for line in log_file:
        if(self.is_pipe_occupancy_line(line)):
          line = line[:-1] #cutting out \n
          pipe_name = self.parse_pipe_name(line)
          pipe_occupancy = self.parse_occupancy(line)
          if(pipe_name in pipe_occupancies):
            pipe_occupancies[pipe_name].append(pipe_occupancy)
          else:
            pipe_occupancies[pipe_name] = [pipe_occupancy]
    for pipe_name, occupancies in pipe_occupancies.items():
      pipe_occupancies[pipe_name] = np.array(occupancies)
    return pipe_occupancies
  
  def parse_scalar_field(self, filename, field_name):
      with open(filename, 'r') as log_file:
        for line in log_file:
          if(field_name in line):
            tokens = line.split(":")
            return tokens[1]
      raise NameError(f"non existent scalar field passed '{field_name}'")

  
  def parse_hitrates(self, filename):
    pipe_hitrates = {}
    with open(filename, 'r') as log_file:
      for line in log_file:
        if(self.is_hitrate_line(line)):
          line = line[:-1] #cutting out \n
          hitrate_dict = self.parse_hitrate(line)
          for hitrate_type, hitrate_value in hitrate_dict.items():
            if hitrate_type in pipe_hitrates.keys():
              pipe_hitrates[hitrate_type] = np.append(pipe_hitrates[hitrate_type], hitrate_value) 
            else:
              pipe_hitrates[hitrate_type] = [hitrate_value]
    return pipe_hitrates

class plotter:
  def plot_dictionaries(self, pipe_dict, title, y_label, print_median = False, udp_success_rate = None, x_label = "time[ms]"):
    
    data_arrays = []
    for index, (key, value) in enumerate(pipe_dict.items()):
      sample_count = value.shape[0]
      x_values = np.arange(value.shape[0]) * 500
      trace = go.Scatter(x=x_values, y=value, mode='lines', name=key, line=dict(width=2, color = PLOT_COLORS[index]))
      data_arrays.append(trace)
      if(print_median):
        median_line = go.Scatter(x= x_values, y = np.repeat(np.median(value[MEDIAN_RELEVANT_SAMPLE_START:MEDIAN_RELEVANT_SAMPLE_END]), sample_count), mode = 'lines', name = 'median ' + key, line = dict(color=PLOT_COLORS[index], width=2, dash='dash'))
        data_arrays.append(median_line)
    if(udp_success_rate != None):
      udp_success_rate_ys = np.repeat(np.median(pipe_dict["Approx input hitrate"]) / udp_success_rate,  sample_count)
      data_arrays.append(go.Scatter(x = np.arange(sample_count) * 500, y = udp_success_rate_ys, mode = 'lines', name = 'Estimated source hit rate',
                                    ))
    
    fig = go.Figure(data = data_arrays)
    # Update the layout for better readability
    fig.update_layout(
        title=title,
        xaxis_title=x_label,
        yaxis_title=y_label,
        template='plotly_white',
         font=dict(size=18)
    )

      # Show the plot
    fig.show()  

class aggregator:
  def remove_numbers(self, line):
    return ''.join(char for char in line if not char.isnumeric())[:-1]
    
  def aggregate_by_pipe_type(self, pipe_dict):
    aggregated_results = {}
    for pipe_name, value in pipe_dict.items():
      new_pipe_name = self.remove_numbers(pipe_name)
      if(new_pipe_name in aggregated_results):
        aggregated_results[new_pipe_name] = np.vstack((aggregated_results[new_pipe_name], value))
      else:
        aggregated_results[new_pipe_name] = value[np.newaxis, :]
    for pipe_name, value in aggregated_results.items():
      mean_value = np.mean(value, axis = 0)
      aggregated_results[pipe_name] = mean_value
      #print(pipe_name)
      #print(aggregated_results[pipe_name])
    return aggregated_results
  
  def aggregate_by_repeats(self, pipe_dicts):
    aggregated_dict = {}
    for key in pipe_dicts[0].keys():
    # Stack arrays along a new axis and calculate mean along that axis
      max_length = max([len(d[key]) for d in pipe_dicts]) 
      aggregated_value = np.mean([np.pad(d[key], (0, max_length - len(d[key])), 'constant') for d in pipe_dicts], axis=0)
      aggregated_dict[key] = aggregated_value
    return aggregated_dict
      
REPEATS = 5
CURRENT = 65
OS_BUFFER = 22
MEDIAN_RELEVANT_SAMPLE_START = 4
MEDIAN_RELEVANT_SAMPLE_END= 24
BASE_FILE_PATH = "build/bin/benchmark_standard_vs_temporal/standard_50kV_65muA_Sn_"
#"build/bin/benchmark_OS_buffer_size/OS_28_boost_17_50kV_65muA_Sn_"
#"build/bin/benchmark_standard_vs_temporal/test_two_step_50kV_65muA_Sn_"
#f"../clusterer_data/benchmark_standard_10s/test_50kV_{CURRENT}muA_Sn_"
#f"build/bin/benchmark_OS_buffer_size/OS_28_boost_17_50kV_65muA_Sn_"

#f"build/bin/benchmark_standard_10s/test_50kV_{CURRENT}muA_Sn_"

#f"build/bin/benchmark_two_step_unsorted/test_50kV_{CURRENT}muA_Sn_"
#f"build/bin/benchmark_printing_10s/test_50kV_{CURRENT}muA_direct_"
#f"build/bin/benchmark_printing_10s/test_50kV_{CURRENT}muA_Sn_"
#f"build/bin/benchmark_two_step_unsorted/test_50kV_{CURRENT}muA_direct_" 
# #f"build/bin/benchmarking_xray_20sec/test_50kV_{CURRENT}muA_"

bench_parser = parser()
agg = aggregator()
repeats_pipe_occupancies = []
repeats_hitrates = []
repeats_received_hits = []
repeats_processed_hits = []
for rep_index in range(REPEATS):
  file_path = BASE_FILE_PATH  + str(rep_index + 1) #+ ".txt"
  
  result = bench_parser.parse_pipe_occupancies(file_path)
  repeats_pipe_occupancies.append(agg.aggregate_by_pipe_type(result))

  hitrates = bench_parser.parse_hitrates(file_path)
  repeats_hitrates.append(hitrates)
  repeats_received_hits.append(int(bench_parser.parse_scalar_field(file_path, "TOTAL HITS RECEIVED")))
  repeats_processed_hits.append(int(bench_parser.parse_scalar_field(file_path, "TOTAL HITS PROCESSED")))
  
    
pipe_occupancies_aggr_by_repeats = agg.aggregate_by_repeats(repeats_pipe_occupancies)
repeats_aggregated = agg.aggregate_by_repeats(repeats_hitrates)

bench_plotter = plotter()
plot_title = BASE_FILE_PATH[BASE_FILE_PATH.rfind("/") + 1:-1]
udp_transfer_rates = [(processed / received) if received!= 0 else float("inf") for received, processed in zip(repeats_received_hits, repeats_processed_hits)]
bench_plotter.plot_dictionaries(pipe_occupancies_aggr_by_repeats, plot_title, "pipe_occupancy[MiB]")
bench_plotter.plot_dictionaries(repeats_aggregated, plot_title, "hitrate[kHit/s]", True) # np.array(udp_transfer_rates).mean())

print("Sent hits from Katherine:")
print(repeats_received_hits)
print("Received hits:")
print(repeats_processed_hits)
print("UDP transfer success rate:")
print([str(100 * processed / received) + "%" if processed != 0 else float("inf") for received, processed in zip(repeats_received_hits, repeats_processed_hits)])
