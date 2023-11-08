import sys
import pandas as pd
import matplotlib.pyplot as plt
import re

# Check if the script has the right number of arguments
if len(sys.argv) != 3:
    print("Usage: python script.py path_to_csv x_column y_column")
    sys.exit(1)

# Command line arguments
csv_file_path = sys.argv[1]
plot_name = sys.argv[2]

#
text_size_big = 20
text_size_medium = 18
text_size_small = 12

# Step 1: Read the CSV file
try:
    df = pd.read_csv(csv_file_path)
except FileNotFoundError:
    print(f"Error: The file {csv_file_path} does not exist.")
    sys.exit(1)
except pd.errors.EmptyDataError:
    print(f"Error: The file {csv_file_path} is empty.")
    sys.exit(1)
except pd.errors.ParserError:
    print(f"Error: The file {csv_file_path} could not be parsed.")
    sys.exit(1)

time_to_ms = 1000000
r = r'BM_SingleEngineBlocking_(.*)_([0-9]*)kB_entropy_(.*)_(.*)_(qpl.*)'
compress_data = {}
decompress_data = {}
for index, row in df.iterrows():
    re_name = re.match(r, row['name'])
    op = re_name.group(1)
    size = (int)(re_name.group(2))
    entropy = (int)(re_name.group(3))
    if not entropy in compress_data:
        compress_data[entropy] = {}
    if not entropy in decompress_data:
        decompress_data[entropy] = {}
    true_entropy = re_name.group(4)
    sw_hw = re_name.group(5)
    time_ms = row['real_time'] / time_to_ms
    compression_ratio = row['Compression Ratio']
    if op == 'Compress':
        if not size in compress_data[entropy]:
            compress_data[entropy][size] = [(), ()]
        if sw_hw == 'qpl_path_software':
            compress_data[entropy][size][0] = (time_ms, compression_ratio, true_entropy)
        elif sw_hw == 'qpl_path_hardware':
            compress_data[entropy][size][1] = (time_ms, compression_ratio, true_entropy)
        else:
            exit(-1)
    elif op == 'DeCompress':
        if not size in decompress_data[entropy]:
            decompress_data[entropy][size] = [0, 0]
        if sw_hw == 'qpl_path_software':
            decompress_data[entropy][size][0] = (time_ms, compression_ratio, true_entropy)
        elif sw_hw == 'qpl_path_hardware':
            decompress_data[entropy][size][1] = (time_ms, compression_ratio, true_entropy)
        else:
            exit(-1)
    else:
        exit(-1)

# Plot #1 - SW vs. HW, single thread, different message sizes, different entropy.
def plot_exp_1(plot_name, compress_data, decompress_data):
    fig, axs = plt.subplots(len(compress_data.keys()), 2, figsize=(12, 16))
    for ax_row, entropy, idx_X in zip(axs, list(compress_data.keys()), range(len(list(compress_data.keys())))):
        for ax, data, title, idx in zip(ax_row, [compress_data[entropy], decompress_data[entropy]], [f'Compression', 'Decompression'], [0, 1]):
            df_raw = pd.DataFrame.from_dict(data, orient='index', columns=['Software', 'Hardware'])
            df = df_raw.map(lambda x: x[0])
            df.reset_index(inplace=True)
            df.rename(columns={'index': 'Key'}, inplace=True)
            df.sort_values('Key', inplace=True)

            df_ratios = df_raw.map(lambda x: x[1])
            df_ratios.reset_index(inplace=True)
            df_ratios.rename(columns={'index': 'Key'}, inplace=True)
            df_ratios.sort_values('Key', inplace=True)

            width = 0.33

            df_left = df[df['Key'] <= 1024]
            df_right = df[df['Key'] > 1024]

            ax_1 = ax.twinx()
            last_x_position = -1
            for sub_ax, df_, hatch_pattern in zip([ax, ax_1], [df_left, df_right], ['', 'x']):
                x_positions = range(last_x_position + 1, last_x_position + 1 + len(df_))
                last_x_position = x_positions[-1]
                sub_ax.bar(x_positions, df_['Software'], width, label='Software', align='center', hatch=hatch_pattern, edgecolor='white', color='gray')
                sub_ax.bar([p + width for p in x_positions], df_['Hardware'], width, label='Hardware', align='center', hatch=hatch_pattern, edgecolor='white', color='darkred')

                # Annotate.
                for i in range(len(df_)):
                    val1 = df_['Software'].iloc[i]
                    val2 = df_['Hardware'].iloc[i]
                    ratio_sw = df_ratios['Software'].iloc[x_positions[i]]
                    ratio_hw = df_ratios['Hardware'].iloc[x_positions[i]]
                    p1 = val1 + sub_ax.get_ylim()[1] / 30
                    p2 = val2 + sub_ax.get_ylim()[1] / 30
                    if (abs(p1 - p2) < 2 * sub_ax.get_ylim()[1] / 30):
                        p1 += 2 * sub_ax.get_ylim()[1] / 30
                    sub_ax.text(x_positions[i], p1, f'{ratio_sw:.1f}',
                            horizontalalignment='center', verticalalignment='center', fontsize=text_size_small, weight='bold')
                    sub_ax.text(x_positions[i] + width, p2, f'{ratio_hw:.1f}',
                            horizontalalignment='center', verticalalignment='center', fontsize=text_size_small, weight='bold')

            ax.set_xticks([p + width / 2 for p in range(len(df))])
            ax.set_xticklabels(df['Key'].astype(str), fontsize=text_size_medium, rotation=45)
            # ax.set_title(title)
            ax.set_xlabel('Data size, kB', fontsize=text_size_big)
            ax.set_yticklabels(ax.get_yticklabels(), fontsize=text_size_medium)
            ax_1.set_yticklabels(ax_1.get_yticklabels(), fontsize=text_size_medium)
            if idx_X == 0:
                ax.set_title(title, fontsize=text_size_big)
            if idx == 0:
                ax.set_ylabel('Time, ms', fontsize=text_size_big)
                ax.legend(loc='upper left', fontsize=text_size_medium)
            ax.grid()

    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#1.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

# Plot #2
def plot_exp_2(plot_name, compress_data, decompress_data):
    data_sw = {}
    data_hw = {}
    for entropy, v in compress_data.items():
        for size, v_data in v.items():
            if not size in data_sw:
                data_sw[size] = {}
            if not size in data_hw:
                data_hw[size] = {}
            sw_time_ms, sw_compression_ratio, sw_true_entropy = v_data[0]
            hw_time_ms, hw_compression_ratio, hw_true_entropy = v_data[1]
            assert(sw_true_entropy == hw_true_entropy)

            data_sw[size][entropy] = [
                sw_compression_ratio, sw_time_ms, decompress_data[entropy][size][0][0], sw_true_entropy
            ]

            data_hw[size][entropy] = [
                hw_compression_ratio, hw_time_ms, decompress_data[entropy][size][1][0], hw_true_entropy
            ]

    fig, axs = plt.subplots(2, len(data_sw), figsize=(48, 6))
    for axes, data, text in zip(axs, [data_sw, data_hw], ['Software', 'Hardware']):
        for ax, (size, entropy_v) in zip(axes, data.items()):
            df = pd.DataFrame.from_dict(entropy_v, orient='index', columns=['Compression ratio', 'Compression time, ms', 'Decompression time, ms', 'True entropy'])
            df.reset_index(inplace=True)
            df.rename(columns={'index': 'Key'}, inplace=True)
            df.sort_values('Key', inplace=True)

            ax_1 = ax.twinx()

            width = 0.35
            x_positions = range(len(df))
            ax.bar(x_positions, df['Compression ratio'], width, label='Software', align='center')
            ax.set_xticks([p + width / 2 for p in range(len(df))])
            ax.set_xticklabels(['{:.2f}'.format(1000 * float(x)) for x in df['True entropy']], rotation=45)
            ax.set_xlabel('Shannon entropy')
            ax.set_title(f'Data size: {size}kB')
            # ax.set_yscale('log')
            ax.set_ylabel(f'{text}: compression ratio')
            ax.grid()

            ax_1.plot(x_positions, df['Compression time, ms'], color='r', label='Compression time')
            ax_1.plot(x_positions, df['Decompression time, ms'], color='black', label='Decompression time')
            ax_1.set_ylabel("Time, ms")
            ax_1.legend()

    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#2.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

# Plot experiments.
# plot_exp_1(plot_name, compress_data, decompress_data)
plot_exp_2(plot_name, compress_data, decompress_data)
