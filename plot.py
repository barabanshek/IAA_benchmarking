import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Check if the script has the right number of arguments
if len(sys.argv) != 3:
    print("Usage: python script.py path_to_csv x_column y_column")
    sys.exit(1)

# Command line arguments
csv_file_path = sys.argv[1]
plot_name = sys.argv[2]
for_paper = True

#
time_ns_to_ms = 1000000
time_us_to_ms = 1000
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

def prepare_data_1(size_filer, entropy_filter):
    r = r'BM_SingleEngineBlocking_(.*)_([0-9]*)kB_entropy_(.*)_(.*)_mode_(.)_(qpl.*)'
    compress_data = {}
    decompress_data = {}
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        op = re_name.group(1)
        size = (int)(re_name.group(2))
        if not size_filer == None and not size in size_filer:
            continue

        entropy = (int)(re_name.group(3))
        if not entropy_filter == None and not entropy in entropy_filter:
            continue

        if not entropy in compress_data:
            compress_data[entropy] = {}
        if not entropy in decompress_data:
            decompress_data[entropy] = {}

        true_entropy = re_name.group(4)
        mode = re_name.group(5)
        sw_hw = re_name.group(6)
        time_ms = row['real_time'] / time_ns_to_ms
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

    return compress_data, decompress_data

# Plot #1 - SW vs. HW, single thread, different message sizes, different entropy.
def plot_exp_1(plot_name, data):
    compress_data, decompress_data = data
    fig, axs = plt.subplots(len(compress_data), 2, figsize=(12, 4 * len(compress_data)))
    for ax_row, entropy, idx_X in zip(axs, list(compress_data), range(len(list(compress_data)))):
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

# Plot #2 - SW vs. HW, single thread, different message sizes, different entropy (another view).
def plot_exp_2(plot_name, data):
    compress_data, decompress_data = data
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

    fig, axs = plt.subplots(2, len(data_sw), figsize=(6 * len(data_sw), 6))
    for axes, data, text, idx_x in zip(axs, [data_sw, data_hw], ['Software', 'Hardware'], [0, 1]):
        for ax, (size, entropy_v), idx_y in zip(axes, data.items(), range(len(data))):
            df = pd.DataFrame.from_dict(entropy_v, orient='index', columns=['Compression ratio', 'Compression time, ms', 'Decompression time, ms', 'True entropy'])
            df.reset_index(inplace=True)
            df.rename(columns={'index': 'Key'}, inplace=True)
            df.sort_values('Key', inplace=True)

            ax_1 = ax.twinx()

            width = 0.43
            x_positions = range(len(df))
            ax.bar(x_positions, df['Compression ratio'], width, label='Software', align='center', color='gray')
            ax.set_yticklabels(ax.get_yticklabels(), fontsize=text_size_medium)
            ax.set_xticks([p + width / 2 for p in range(len(df))])
            ax.set_xticklabels(['{:.2f}'.format(1 * float(x)) for x in df['True entropy']], fontsize=text_size_medium, rotation=45)
            if idx_x == 1:
                ax.set_xlabel('Shannon entropy', fontsize=text_size_big)
            if idx_x == 0:
                ax.set_title(f'Data size: {size}kB', fontsize=text_size_big)
            ax.grid()
            # ax.set_yscale('log')
            if idx_y == 0:
                ax.set_ylabel(f'Compression \n ratio', fontsize=text_size_big)
            if idx_y == 1:
                ax_1.set_ylabel("Time, ms", fontsize=text_size_big)

            ax_1.plot(x_positions, df['Compression time, ms'], label='Compression time', color='black', marker='o')
            ax_1.plot(x_positions, df['Decompression time, ms'], label='Decompression time', color='darkred', marker='o')
            ax_1.set_yticklabels(ax_1.get_yticklabels(), fontsize=text_size_medium)
            if idx_x == 0 and idx_y == 0:
                ax_1.legend(fontsize=text_size_small)

    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#2.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

def prepare_and_plot_exp_3(plot_name, size_filer, entropy_filter, entropy_filter_text):
    r = r'BM_SingleEngineBlocking_SoftwareCompress_HardwareDecompress_([0-9]*)kB_entropy_(.*)_(.*)_level_([0-9])'
    data = {}
    compression_levels = []
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        size = (int)(re_name.group(1))
        if not size_filer == None and not size in size_filer:
            continue

        entropy = (int)(re_name.group(2))
        if not entropy_filter == None and not entropy in entropy_filter:
            continue

        compression_level = (int)(re_name.group(4))
        compression_ratio = row['Compression Ratio']
        compression_time = row['Compression Time'] / time_us_to_ms
        decompression_time = row['real_time'] / time_ns_to_ms

        if not compression_level in compression_levels:
            compression_levels.append(compression_level)
        if not entropy in data:
            data[entropy] = {}
        if not size in data[entropy]:
            data[entropy][size] = {}
        data[entropy][size][compression_level] = [compression_ratio, compression_time, decompression_time]

    # plot.
    fig, axs = plt.subplots(1, len(data), figsize=(6 * len(data), 4))
    for (entropy, entropy_v), ax, idx in zip(data.items(), axs, range(len(data))):
        ax_1 = ax.twinx()

        o = 0
        plots = []
        for c_lvl, hatch_pattern in zip(compression_levels, ['', 'x']):
            new_dict = {}
            for k, v in entropy_v.items():
                new_dict[k] = v[c_lvl]
            df_raw = pd.DataFrame.from_dict(new_dict, orient='index', columns=['Compression ratio', 'Compression time', 'Decompression time'])
            df_raw.reset_index(inplace=True)
            df_raw.rename(columns={'index': 'Key'}, inplace=True)
            df_raw.sort_values('Key', inplace=True)

            width = 0.15
            x_positions = range(len(df_raw))
            pl = ax.bar([p + o for p in x_positions], df_raw['Compression ratio'], width, align='center', color='gray', label=f'Compr. ratio, level {c_lvl}', hatch=hatch_pattern)
            plots.append(pl)
            pl = ax_1.bar([p + o + width for p in x_positions], df_raw['Decompression time'], width, align='center', color='darkred', label=f'Decompr. time, level {c_lvl}', hatch=hatch_pattern)
            plots.append(pl)
            ax.set_xticks([p + width for p in range(len(df_raw))])
            ax.set_xticklabels(df_raw['Key'].astype(str), fontsize=text_size_medium)
            if not entropy_filter == None:
                ax.set_title(f'Entropy: {entropy_filter_text[idx]}', fontsize=text_size_big)
            else:
                ax.set_title(f'Entropy: {entropy}', fontsize=text_size_big)
            ax.set_xlabel('Data size, kB', fontsize=text_size_big)
            ax.set_yticklabels(ax.get_yticklabels(), fontsize=text_size_medium)
            ax_1.set_yticklabels(ax_1.get_yticklabels(), fontsize=text_size_medium)

            if idx == 0:
                ax.set_ylabel('Compression ratio', fontsize=text_size_big)
            if idx == len(data) - 1:
                ax_1.set_ylabel('Decompression time, \n ms', fontsize=text_size_big)

            o += 2 * width

        ax.grid()
        # ax_1.grid()

        # TODO: fix it
        # if idx == 0:
            # labs = [l.get_label() for l in plots]
            # ax.legend(plots, labs, ncol=2, bbox_to_anchor=(0.3, 1.3), fontsize=text_size_small)

    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#3.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

def prepare_and_plot_exp_4_5(exp, r, plot_name, size_filer, entropy_filter, mode_names):
    data = {}
    modes = []
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue
        op = re_name.group(1)
        size = (int)(re_name.group(2))
        if not size_filer == None and not size in size_filer:
            continue
        entropy = (int)(re_name.group(3))
        if not entropy_filter == None and not entropy in entropy_filter:
            continue
        true_entropy = re_name.group(4)
        mode = re_name.group(5)
        if not mode in modes:
            modes.append(mode)

        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if not entropy in data:
            data[entropy] = {}
        if not size in data[entropy]:
            data[entropy][size] = {}
        if not mode in data[entropy][size]:
            data[entropy][size][mode] = [compression_ratio, true_entropy, 0, 0]

        if op == 'Compress':
            data[entropy][size][mode][2] = time_ms
        elif op == 'DeCompress':
            data[entropy][size][mode][3] = time_ms
        else:
            exit(0)

    # plot.
    L = len(list(data.values())[0])
    fig, axs = plt.subplots(len(data), L, figsize=(7 * len(data), 3.5 * L))
    axs = np.transpose(axs)
    for (entropy, entropy_v), ax_s, idx in zip(data.items(), axs, range(len(axs))):
        for (size, size_v), ax, idx_y in zip(entropy_v.items(), ax_s, range(len(ax_s))):
            true_entropy = float(list(size_v.values())[0][1])
            ax_1 = ax.twinx()

            df_raw = pd.DataFrame.from_dict(size_v, orient='index', columns=['Compression ratio', 'True Entropy', 'Compression time', 'Decompression time'])
            df_raw.reset_index(inplace=True)
            df_raw.rename(columns={'index': 'Key'}, inplace=True)
            df_raw.sort_values('Key', inplace=True)
            width = 0.1
            x_positions = [t / 2 for t in range(len(df_raw))]
            b1 = ax.bar(x_positions, df_raw['Compression ratio'], width, align='center', label=f'Ratio', color='gray')
            b2 = ax_1.bar([p + width for p in x_positions], df_raw['Compression time'], width, align='center', label=f'Compr. time', color='darkgoldenrod')
            b3 = ax_1.bar([p + 2 * width for p in x_positions], df_raw['Decompression time'], width, align='center', label=f'Decompr. time', color='darkred')
            ax.set_xticks([p + width for p in x_positions])
            ax.set_xticklabels(mode_names, fontsize=text_size_medium, rotation=0)
            ax.set_yticklabels(ax.get_yticklabels(), fontsize=text_size_medium)
            ax_1.set_yticklabels(ax_1.get_yticklabels(), fontsize=text_size_medium)
            ax.set_title('Size(MB)/ Entropy: ' + f'{size / 1024:.1f}/ {true_entropy:.5f}', fontsize=text_size_big)

            if idx == 0:
                ax.set_ylabel('Compression ratio', fontsize=text_size_big)
            if idx == len(ax_s) - 1:
                ax_1.set_ylabel('Time, ms', fontsize=text_size_big)

            # Annotate.
            def add_value_labels(bars, axis):
                for bar in bars:
                    height = bar.get_height()
                    axis.text(bar.get_x() + bar.get_width() / 2., 0.6002 * height, f'{height:.1f}', ha='center', va='bottom', fontsize=text_size_big, rotation=90)
            add_value_labels(b1, ax)
            add_value_labels(b2, ax_1)
            add_value_labels(b3, ax_1)

            if idx == 0 and idx_y == 0:
                plots = [b1, b2, b3]
                labs = [l.get_label() for l in plots]
                # ax.legend(plots, labs, ncol=1, fontsize=text_size_small, loc='upper left')
            ax.grid()


    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#{exp}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

def prepare_and_plot_exp_4(plot_name, size_filer, entropy_filter):
    r = r'BM_SingleEngineBlocking_(.*)_([0-9]*)kB_entropy_(.*)_(.*)_mode_(.)_qpl_path_hardware'
    prepare_and_plot_exp_4_5(4, r, plot_name, size_filer, entropy_filter, ['Fixed Block', 'Dynamic Block', 'Static Block'])

def prepare_and_plot_exp_5(plot_name, size_filer, entropy_filter):
    r = r'BM_SingleEngineBlocking_(.*)_Canned_([0-9]*)kB_entropy_(.*)_(.*)_mode_(.)'
    prepare_and_plot_exp_4_5(5, r, plot_name, size_filer, entropy_filter, ['Continious \nbaseline', 'Naive Dynamic \nBlock', 'Canned'])

def prepare_and_plot_exp_6(plot_name, size_filer, entropy_filter):
    r = r'BM_MultipleEngine_(.*)_([0-9]*)kB_entropy_(.*)_(.*)_jobs_(.*)_mode_(.*)'
    data = {}
    modes = []
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue
        op = re_name.group(1)
        size = (int)(re_name.group(2))
        if not size_filer == None and not size in size_filer:
            continue
        entropy = (int)(re_name.group(3))
        if not entropy_filter == None and not entropy in entropy_filter:
            continue
        true_entropy = re_name.group(4)

        job_n = (int)(re_name.group(5))
        mode = re_name.group(6)

        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if not entropy in data:
            data[entropy] = {}
        if not size in data[entropy]:
            data[entropy][size] = {}
        if not mode in data[entropy][size]:
            data[entropy][size][mode] = {}
        if not job_n in data[entropy][size][mode]:
            data[entropy][size][mode][job_n] = [compression_ratio, true_entropy, 0, 0]

        if op == 'Compress':
            data[entropy][size][mode][job_n][2] = time_ms
        elif op == 'DeCompress':
            data[entropy][size][mode][job_n][3] = time_ms
        else:
            exit(0)

    L = len(list(data.values())[0])
    fig, axs = plt.subplots(len(data), L, figsize=(6 * len(data), 3.5 * L))
    # axs = np.transpose(axs)
    for (entropy, entropy_v), ax_s, idx in zip(data.items(), axs, range(len(axs))):
        for (size, size_v), ax, idx_y in zip(entropy_v.items(), ax_s, range(len(ax_s))):
            ax_1 = ax.twinx()

            plots = []
            for (mode, mode_v), mode_name, l in zip(size_v.items(), ['', ', serial Huffman'], ['-', '--']):
                true_entropy = float(list(mode_v.values())[0][1])
                df_raw = pd.DataFrame.from_dict(mode_v, orient='index', columns=['Compression ratio', 'True Entropy', 'Compression time', 'Decompression time'])
                df_raw.reset_index(inplace=True)
                df_raw.rename(columns={'index': 'Key'}, inplace=True)
                df_raw.sort_values('Key', inplace=True)

                x_positions = [t for t in range(len(df_raw))]
                plots = plots + ax.plot(x_positions, df_raw['Compression time'], label=f'Compression time {mode_name}', color='black', marker='o', linestyle=l)
                plots = plots + ax_1.plot(x_positions, df_raw['Decompression time'], label=f'DeCompression time {mode_name}', color='darkred', marker='o', linestyle=l)

            ax.set_xticks([p for p in x_positions])
            ax.set_xticklabels(df_raw['Key'], fontsize=text_size_medium, rotation=45)
            ax.set_yticklabels(ax.get_yticklabels(), fontsize=text_size_medium)
            ax_1.set_yticklabels(ax_1.get_yticklabels(), fontsize=text_size_medium)
            ax.set_title('Size(MB)/ Entropy: ' + f'{size / 1000:.1f}/ {true_entropy:.5f}', fontsize=text_size_big)
            # ax.set_title('Data size: ' + f'{size / 1024:.0f}MB', fontsize=text_size_big)

            ax.set_xlabel('Number of jobs', fontsize=text_size_big)
            # if idx_y == 0:
            ax.set_ylabel('Time, ms', fontsize=text_size_big)
            ax.grid()
            plots = plots[:3]

            if idx == 0 and idx_y == 0:
                labs = [l.get_label() for l in plots]
                ax.legend(plots, labs, ncol=1, fontsize=text_size_small, loc='upper right')

    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#{6}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

def prepare_and_plot_exp_7(plot_name, size_filer, entropy_filter):
    r = r'BM_SingleEngineMinorPageFault_(.*)_([0-9]*)kB_entropy_(.*)_(.*)_pfscenario_(.*)'
    data = {}
    modes = []
    pfscenarios = {0: 'Major \npf', 1: 'Minor \npf', 2: 'ATS \nmiss', 3: 'No \nfaults'}
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue
        op = re_name.group(1)
        size = (int)(re_name.group(2))
        if not size_filer == None and not size in size_filer:
            continue
        entropy = (int)(re_name.group(3))
        if not entropy_filter == None and not entropy in entropy_filter:
            continue
        true_entropy = re_name.group(4)
        pf_scenario = (int)(re_name.group(5))
        if not pf_scenario in pfscenarios:
            exit(0)

        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if not entropy in data:
            data[entropy] = {}
        if not size in data[entropy]:
            data[entropy][size] = {}
            data[entropy][size]['compress'] = []
            data[entropy][size]['decompress'] = []

        if op == 'Compress':
            data[entropy][size]['compress'].append(time_ms)
        elif op == 'DeCompress':
            data[entropy][size]['decompress'].append(time_ms)
        else:
            exit(0)

    L = len(list(data.values())[0])
    fig, axs = plt.subplots(len(data), L, figsize=(6.5 * len(data), 3.5 * L))
    # axs = np.transpose(axs)
    for (entropy, entropy_v), ax_s, idx_x in zip(data.items(), axs, range(len(axs))):
        for (size, size_v), ax, idx_y in zip(entropy_v.items(), ax_s, range(len(ax_s))):
            ax_1 = ax.twinx()

            bars = []
            for (op, op_v), axx, c, idx, l in zip(size_v.items(), [ax, ax_1], ['gray', 'darkred'], range(len(size_v.items())), ['Compression', 'Decompression']):
                width = 0.23
                x_positions = [t + width * idx for t in range(len(pfscenarios.values()))]
                b = axx.bar(x_positions, op_v, width, align='center', color=c, label=l)
                bars.append(b)

                # Annotate.
                def add_value_labels(bars, axis):
                    for bar in bars:
                        height = bar.get_height()
                        axis.text(bar.get_x() + bar.get_width() / 2., 0.3002 * height, f'{height:.3f}', ha='center', va='bottom', fontsize=text_size_big, rotation=90)
                add_value_labels(b, axx)

            ax.set_xticks([t for t in range(len(pfscenarios.values()))])
            ax.set_xticklabels(pfscenarios.values(), fontsize=text_size_medium, rotation=0)
            ax.set_yticklabels(ax.get_yticklabels(), fontsize=text_size_medium)
            ax_1.set_yticklabels(ax_1.get_yticklabels(), fontsize=text_size_medium)
            # ax.set_title('Size(kB)/ Entropy: ' + f'{size:.3f}/ {entropy:.5f}', fontsize=text_size_big)
            ax.set_title('Data size: ' + f'{size:.0f}kB', fontsize=text_size_big)
            ax.grid()

            # ax.set_xlabel('Number of jobs', fontsize=text_size_big)
            if idx_y == 0:
                ax.set_ylabel('Time, ms', fontsize=text_size_big)

            if idx_y == 1:
                labs = [l.get_label() for l in bars]
                ax.legend(bars, labs, ncol=1, fontsize=text_size_medium, loc='upper right')

    for r in ['png', 'pdf']:
        plot_name = f'{plot_name}_#{7}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name}")

#
# Plot experiments.
#
if for_paper:
    plot_exp_1(plot_name, prepare_data_1(None, [1, 400]))
    plot_exp_2(plot_name, prepare_data_1([256, 4], None))
    prepare_and_plot_exp_3(plot_name, [16384, 65536, 262144], [5, 400], ['low', 'high'])
    prepare_and_plot_exp_4(plot_name, [262144, 16384], [5, 200])
    prepare_and_plot_exp_6(plot_name, [524288, 1024], [5, 200])
    prepare_and_plot_exp_7(plot_name, [262144, 1024], [300, 400])
else:
    plot_exp_1(plot_name, prepare_data_1(None, None))
    plot_exp_2(plot_name, prepare_data_1(None, None))
    prepare_and_plot_exp_3(plot_name, None, None, None)
    prepare_and_plot_exp_4(plot_name, None, None)
    prepare_and_plot_exp_7(plot_name, None, None)
