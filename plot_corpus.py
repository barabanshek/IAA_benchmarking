from enum import Enum
import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import re

from matplotlib.ticker import FormatStrFormatter

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
text_size_ultrabig = 26
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

def prepare_data_1(mode_filter, b_name_filter):
    r = r'BM_SingleEngineBlocking_(.*)_([0-9]*)kB_name_(.*)_entropy_(.*)_mode_(.)_(qpl.*)_mean'
    compress_data = {}
    decompress_data = {}
    compress_data['Software'] = {}
    compress_data['Hardware'] = {}
    decompress_data['Software'] = {}
    decompress_data['Hardware'] = {}
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        op = re_name.group(1)
        size = (int)(re_name.group(2)) / (1024)

        b_name = re_name.group(3)
        b_name = b_name.replace('dataset/silesia_tmp/', '')
        b_name = b_name.replace('dataset/snapshots_tmp/', '')
        if not b_name_filter == None and not b_name in b_name_filter:
            continue

        if not b_name in compress_data['Software']:
            compress_data['Software'][b_name] = {}
            compress_data['Hardware'][b_name] = {}
        if not b_name in decompress_data['Software']:
            decompress_data['Software'][b_name] = {}
            decompress_data['Hardware'][b_name] = {}

        entropy = re_name.group(4)
        mode = re_name.group(5)
        if not (int)(mode) == mode_filter:
            continue

        sw_hw = re_name.group(6)
        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if op == 'Compress':
            if sw_hw == 'qpl_path_software':
                compress_data['Software'][b_name] = (time_ms, compression_ratio, entropy, size)
            elif sw_hw == 'qpl_path_hardware':
                compress_data['Hardware'][b_name] = (time_ms, compression_ratio, entropy, size)
            else:
                exit(-1)
        elif op == 'DeCompress':
            if sw_hw == 'qpl_path_software':
                decompress_data['Software'][b_name] = (time_ms, compression_ratio, entropy, size)
            elif sw_hw == 'qpl_path_hardware':
                decompress_data['Hardware'][b_name] = (time_ms, compression_ratio, entropy, size)
            else:
                exit(-1)
        else:
            exit(-1)

    return compress_data, decompress_data

# Plot #1 - SW vs. HW, single thread, different message sizes, different b_name.
def plot_exp_1(plot_name, data, name_suffix, omit_x_labels=False, omit_y1_axis=False, omit_y2_axis=False, omit_legend=False):
    compress_data = data[0]
    fig, ax = plt.subplots(1, 1, figsize=(len(compress_data['Software']) * 1.5, 4.5))
    ax_1 = ax.twinx()

    data_software = compress_data['Software']
    data_hardware = compress_data['Hardware']

    df_software = pd.DataFrame.from_dict(data_software, orient='index', columns=['Time', 'Ratio', 'Entropy', 'Size'])
    df_software.reset_index(inplace=True)
    df_software.rename(columns={'index': 'Key'}, inplace=True)
    df_software.sort_values('Size', inplace=True)
    df_hardware = pd.DataFrame.from_dict(data_hardware, orient='index', columns=['Time', 'Ratio', 'Entropy', 'Size'])
    df_hardware.reset_index(inplace=True)
    df_hardware.rename(columns={'index': 'Key'}, inplace=True)
    df_hardware.sort_values('Size', inplace=True)

    width = 0.33

    x_positions = range(0, len(df_software))
    ax.bar(x_positions, df_software['Time'], width, label='Time, software', align='center', edgecolor='white', color='gray')
    ax.bar([x + width for x in x_positions], df_hardware['Time'], width, label='Time, hardware', align='center', edgecolor='white', color='darkred')
    ax_1.plot(x_positions, df_software['Ratio'], label='Compression ratio, software', color='black', marker='o', linestyle='--')
    ax_1.plot(x_positions, df_hardware['Ratio'], label='Compression ratio, hardware', color='darkred', marker='o', linestyle='--')

    # Annotate
    time_differences = df_software['Time'] / df_hardware['Time']
    for x, y1, y2, sw_ratio, hw_ratio, time_diff in zip(x_positions, df_software['Time'], df_hardware['Time'], df_software['Ratio'], df_hardware['Ratio'], time_differences):
        ax.annotate(f"{time_diff:.1f}x", (x + width, y2 + (int)(ax.get_ylim()[1] * 0.03)), ha='center', fontsize=text_size_small, color='darkred', weight='bold', rotation=90)

    ax.set_xticks([p for p in range(len(df_software))])
    if not omit_x_labels:
        ax.set_xticklabels(
            [f'{row["Key"]}\n({row["Size"]:.1f} MB)' for index, row in df_software.iterrows()],
            fontsize=text_size_medium, rotation=45)
    else:
        ax.set_xticklabels([])

    ax.set_title(f'{name_suffix} compression', fontsize=text_size_big)
    ax.yaxis.set_tick_params(labelsize=text_size_medium, rotation=0)

    if not omit_y1_axis:
        ax.set_ylabel('Time, ms', fontsize=text_size_big)

    if not omit_y2_axis:
        ax_1.set_ylabel('Compression ratio', fontsize=text_size_big)
    ax_1.yaxis.set_tick_params(labelsize=text_size_medium, rotation=0)

    if not omit_legend:
        ax.legend(loc='upper left', fontsize=text_size_medium)
        ax_1.legend(loc='upper center', fontsize=text_size_medium)

    ax.grid()

    for r in ['png', 'pdf']:
        plot_name_ = f'{plot_name}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name_}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name_}")

def plot_exp_1_a(plot_name, data, name_suffix, omit_x_labels=False, omit_y1_axis=False, omit_legend=False):
    decompress_data = data[1]
    fig, ax = plt.subplots(1, 1, figsize=(len(decompress_data['Software']) * 0.7, 4.5))

    data_software = decompress_data['Software']
    data_hardware = decompress_data['Hardware']

    df_software = pd.DataFrame.from_dict(data_software, orient='index', columns=['Time', 'Ratio', 'Entropy', 'Size'])
    df_software.reset_index(inplace=True)
    df_software.rename(columns={'index': 'Key'}, inplace=True)
    df_software.sort_values('Size', inplace=True)
    df_hardware = pd.DataFrame.from_dict(data_hardware, orient='index', columns=['Time', 'Ratio', 'Entropy', 'Size'])
    df_hardware.reset_index(inplace=True)
    df_hardware.rename(columns={'index': 'Key'}, inplace=True)
    df_hardware.sort_values('Size', inplace=True)

    x_positions = range(0, len(df_software))
    ax.plot(x_positions, df_software['Time'], label='Time, software', color='black', marker='o', linewidth=2)
    ax.plot(x_positions, df_hardware['Time'], label='Time, hardware', color='darkred', marker='o', linewidth=2)

    ax.set_xticks([p for p in range(len(df_software))])
    if not omit_x_labels:
        ax.set_xticklabels(
            [f'{row["Key"]}' for index, row in df_software.iterrows()],
            fontsize=text_size_medium, rotation=45)
    else:
        ax.set_xticklabels([])

    ax.set_title(f'{name_suffix} decompression', fontsize=text_size_big)
    ax.yaxis.set_tick_params(labelsize=text_size_medium, rotation=0)
    ax.set_yscale('log')

    if not omit_y1_axis:
        ax.set_ylabel('Time, ms', fontsize=text_size_big)

    if not omit_legend:
        ax.legend(loc='upper left', fontsize=text_size_medium)

    ax.grid()

    for r in ['png', 'pdf']:
        plot_name_ = f'{plot_name}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name_}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name_}")

def prepare_and_plot_exp_2(plot_name, b_name_filter):
    r = r'BM_SingleEngineBlocking_(.*)_Canned_([0-9]*)kB_name_(.*)_entropy_(.*)_mode_(.)_mean'
    data = {}
    modes = []
    mode_names = ['Continious\nbaseline', 'Naive', 'Canned']
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        op = re_name.group(1)
        size = (int)(re_name.group(2))

        b_name = re_name.group(3)
        b_name = b_name.replace('dataset/silesia_tmp/', '')
        b_name = b_name.replace('dataset/snapshots_tmp/', '')
        if not b_name_filter == None and not b_name in b_name_filter:
            continue

        entropy = re_name.group(4)
        mode = re_name.group(5)
        if not mode in modes:
            modes.append(mode)

        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if not b_name in data:
            data[b_name] = {}

        if not mode in data[b_name]:
            data[b_name][mode] = [compression_ratio, entropy, 0, 0, size]

        if op == 'Compress':
            data[b_name][mode][2] = time_ms
        elif op == 'DeCompress':
            data[b_name][mode][3] = time_ms
        else:
            exit(0)

    # plot.
    fig, axs = plt.subplots(1, len(data), figsize=(len(data) * 8, 5.5))
    for (b_name, b_data), ax, id_x in zip(data.items(), axs, range(len(data))):
        b_size_kb = b_data['0'][4]
        ax_1 = ax.twinx()

        df_raw = pd.DataFrame.from_dict(b_data, orient='index', columns=['Compression ratio', 'Entropy', 'Compression time', 'Decompression time', 'Size'])        
        df_raw.reset_index(inplace=True)
        df_raw.rename(columns={'index': 'Key'}, inplace=True)
        df_raw.sort_values('Key', inplace=True)

        width = 0.23
        x_positions = [t for t in range(len(df_raw))]
        b1 = ax.bar(x_positions, df_raw['Compression ratio'], width, align='center', label=f'Ratio', color='gray')
        b2 = ax_1.bar([p + width for p in x_positions], df_raw['Compression time'], width, align='center', label=f'Compression time', color='darkgoldenrod')
        b3 = ax_1.bar([p + 2 * width for p in x_positions], df_raw['Decompression time'], width, align='center', label=f'Decompression time', color='darkred')

        ax.set_xticks([p + width for p in x_positions])
        ax.set_xticklabels(mode_names, fontsize=text_size_ultrabig, rotation=0)
        ax.yaxis.set_tick_params(labelsize=text_size_ultrabig, rotation=0)
        ax_1.set_ylim(0, df_raw['Compression time'][2] + df_raw['Compression time'][2] * 1)
        ax_1.yaxis.set_tick_params(labelsize=text_size_ultrabig, rotation=0)
        ax.set_title(f'{b_name}\n({(b_size_kb/4):.0f} 4kB pages)', fontsize=text_size_ultrabig+4)
        if id_x == 0:
            ax.set_ylabel('Compression ratio', fontsize=text_size_ultrabig)
        if id_x == 1:
            ax_1.set_ylabel('Time, ms', fontsize=text_size_ultrabig)

        # Annotate.
        def add_value_labels(bars, axis):
            for bar in bars:
                height = bar.get_height()
                value = height
                if height > axis.get_ylim()[1]:
                    height = axis.get_ylim()[1]
                axis.text(bar.get_x() + bar.get_width() / 2., 0.5002 * height, f'{value:.1f}', ha='center', va='bottom', fontsize=text_size_ultrabig, rotation=90)
        add_value_labels(b1, ax)
        add_value_labels(b2, ax_1)
        add_value_labels(b3, ax_1)

        # if id_x == 0:
            # plots = [b1, b2, b3]
            # labs = [l.get_label() for l in plots]
            # ax.legend(plots, labs, ncol=3, fontsize=text_size_ultrabig+3, bbox_to_anchor=(1, 2), loc='upper center')
        ax.grid()

    for r in ['png', 'pdf']:
        plot_name_ = f'{plot_name}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name_}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name_}")

def prepare_and_plot_exp_3(plot_name, b_name_filter, mode_filter):
    r = r'BM_MultipleEngine_(.*)_([0-9]*)kB_name_(.*)_entropy_(.*)_jobs_(.*)_mode_(.*)_mean'
    data = {}
    modes = []
    mode_names = ['Fixed Block', 'Dynamic Block', 'Static Block']
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        op = re_name.group(1)
        size = (int)(re_name.group(2))

        b_name = re_name.group(3)
        b_name = b_name.replace('dataset/silesia_tmp/', '')
        b_name = b_name.replace('dataset/snapshots_tmp/', '')
        if not b_name_filter == None and not b_name in b_name_filter:
            continue

        entropy = re_name.group(4)

        job_n = (int)(re_name.group(5))
        mode = (int)(re_name.group(6))
        if mode_filter and not mode in mode_filter:
            continue
        if not mode in modes:
            modes.append(mode)

        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if not b_name in data:
            data[b_name] = {}

        if not mode in data[b_name]:
            data[b_name][mode] = {}

        if not job_n in data[b_name][mode]:
            data[b_name][mode][job_n] = [compression_ratio, entropy, 0, 0, size]

        if op == 'Compress':
            data[b_name][mode][job_n][2] = (size / time_ms) * 1000 / (1024 * 1024)
        elif op == 'DeCompress':
            data[b_name][mode][job_n][3] = (size / time_ms) * 1000 / (1024 * 1024)
        else:
            exit(0)

    fig, axs = plt.subplots(len(data), len(modes), figsize=(len(modes) * 5.5, len(data) * 3.5))
    for (b_name, b_data), ax_s, id_x in zip(data.items(), axs, range(len(axs))):
        for (m_name, m_data), ax in zip(b_data.items(), ax_s):
            size = m_data[2][4] / 1024
            df_raw = pd.DataFrame.from_dict(m_data, orient='index', columns=['Compression ratio', 'Entropy', 'Compression time', 'Decompression time', 'Size'])
            df_raw.reset_index(inplace=True)
            df_raw.rename(columns={'index': 'Key'}, inplace=True)
            df_raw.sort_values('Key', inplace=True)

            x_positions = [t for t in range(len(df_raw))]
            ax.plot(x_positions, df_raw['Compression time'], label=f'Compression', color='black', marker='o', linewidth=2)
            ax.plot(x_positions, df_raw['Decompression time'], label=f'DeCompression', color='darkred', marker='o', linewidth=2)

            ax.yaxis.set_major_locator(ticker.MultipleLocator(5))
            ax.set_xticks([p for p in x_positions])
            ax.set_xticklabels(df_raw['Key'], fontsize=text_size_medium, rotation=45)
            ax.yaxis.set_tick_params(labelsize=text_size_medium, rotation=0)
            ax.set_title(f'{b_name} ({size:.1f} MB)/{mode_names[m_name]}', fontsize=text_size_big)
            if id_x == 2:
                ax.set_xlabel('Number of jobs', fontsize=text_size_big)
            ax.grid()

        ax_s[0].set_ylabel('Throughput, GB/s', fontsize=text_size_big)
        # if id_x == 0:
            # ax_s[0].legend(ncol=2, fontsize=text_size_ultrabig, bbox_to_anchor=(1, 2), loc='upper left')

    for r in ['png', 'pdf']:
        plot_name_ = f'{plot_name}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name_}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name_}")

def prepare_and_plot_exp_4(plot_name, b_name_filter):
    r = r'BM_SingleEngineMinorPageFault_(.*)_([0-9]*)kB_name_(.*)_entropy_(.*)_pfscenario_(.*)_mean'
    data = {}
    modes = []
    pfscenarios = {0: 'Major \npf', 1: 'Minor \npf', 2: 'ATS \nmiss', 3: 'No \nfaults'}
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        op = re_name.group(1)
        size = (int)(re_name.group(2))

        b_name = re_name.group(3)
        b_name = b_name.replace('dataset/silesia_tmp/', '')
        b_name = b_name.replace('dataset/snapshots_tmp/', '')
        if not b_name_filter == None and not b_name in b_name_filter:
            continue

        entropy = re_name.group(4)
        pf_scenario = (int)(re_name.group(5))
        if not pf_scenario in pfscenarios:
            exit(0)

        time_ms = row['real_time'] / time_ns_to_ms
        compression_ratio = row['Compression Ratio']

        if not b_name in data:
            data[b_name] = {}
            data[b_name]['compress'] = []
            data[b_name]['decompress'] = []

        if op == 'Compress':
            data[b_name]['compress'].append(time_ms)
        elif op == 'DeCompress':
            data[b_name]['decompress'].append(time_ms)
        else:
            exit(0)

    fig, axs = plt.subplots(1, len(data), figsize=(6.7 * len(data), 5))
    for (b_name, b_data), ax, id_x in zip(data.items(), axs, range(len(axs))):
        ax_1 = ax.twinx()

        bars = []
        for (op, op_v), axx, c, idx, l in zip(b_data.items(), [ax, ax_1], ['gray', 'darkred'], range(len(b_data.items())), ['Compression', 'Decompression']):
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
        ax.set_xticklabels(pfscenarios.values(), fontsize=text_size_ultrabig+2, rotation=0)
        ax.yaxis.set_tick_params(labelsize=text_size_big, rotation=0)
        ax_1.yaxis.set_tick_params(labelsize=text_size_big, rotation=0)
        ax.set_title(f'{b_name}', fontsize=text_size_ultrabig)

        ax.grid()
        if id_x == 0:
            ax.set_ylabel('Time, ms', fontsize=text_size_ultrabig)
        # if id_x == 1:
            # ax_1.set_ylabel('Decompression time, ms', fontsize=text_size_big)
        # labs = [l.get_label() for l in bars]
        # if id_x == 0:
            # ax.legend(bars, labs, ncol=2, fontsize=text_size_medium, bbox_to_anchor=(1, 2), loc='upper center')

    for r in ['png', 'pdf']:
        plot_name_ = f'{plot_name}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name_}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name_}")

def prepare_and_plot_exp_5(plot_name):
    r = r'BM_FullSystem_([0-9]*)kB_mode_(.*)_mean'
    data = {}
    modes = ['Disk Read', 'Disk Read (O_DIRECT)', 'Decompress', 'Disk Read + Decompress']
    for index, row in df.iterrows():
        re_name = re.match(r, row['name'])
        if re_name == None:
            continue

        # size = (int)(re_name.group(1))
        mode = (int)(re_name.group(2))

        time_ms = row['real_time'] / time_ns_to_ms
        file_size = (int)(row['File Size'])

        if not mode in data:
            data[mode] = {}

        data[mode][file_size] = (file_size/time_ms) * 1000 / (1024 * 1024)

    fig, ax = plt.subplots(1, 1, figsize=(8.5, 4.5))
    arr = np.zeros((1, len(list(data.values())[0].values())))
    for (mode, mode_v), mode_name, c, l, m in zip(data.items(), modes, ['gray', 'black', 'black', 'darkred'], ['-', '--', '-', '-'], ['x', 'o', 'o', 'o']):
        arr = np.vstack((arr, list(mode_v.values())))
        df_raw = pd.DataFrame.from_dict(mode_v, orient='index', columns=['Bandwidth'])
        df_raw.reset_index(inplace=True)
        df_raw.rename(columns={'index': 'Key'}, inplace=True)
        df_raw.sort_values('Key', inplace=True)

        x_positions = [t for t in range(len(df_raw))]
        ax.plot(x_positions, df_raw['Bandwidth'], color=c, marker=m, label=mode_name, linestyle=l, linewidth=2)
        ax.set_xticks(x_positions)
        ax.set_xticklabels(['{:.2f}'.format(float(x) / (1024 * 1024)) for x in df_raw['Key']], fontsize=text_size_medium, rotation=45)

    if len(data) >= 2:
        first_line = list(data.values())[0].values()
        third_line = list(data.values())[2].values()

        for x, (y1, y3) in enumerate(zip(first_line, third_line)):
                diff = abs(y3-y1)*100/y1
                xx = len(first_line) - x - 1
                if xx > 5 and diff > 20:
                    ylim = ax.get_ylim()
                    y_range = ylim[1] - ylim[0]
                    # ax.axvline(x=xx, ymin=(y3 - ylim[0]) / y_range - 0.1, ymax=0.4, color='black', linestyle='--')
                    # ax.annotate('', xy=(xx, y1), xytext=(xx, y3), 
                    #             arrowprops=dict(facecolor='black', arrowstyle="<->", linestyle='--'))
                    # # Add text to show the difference
                    # ax.text(xx, y_range * 0.45, f'{diff:.0f}%', horizontalalignment='center', verticalalignment='bottom', fontsize=text_size_small, color='black')

    ax.yaxis.set_tick_params(labelsize=text_size_ultrabig, rotation=0)
    ax.set_ylabel('Achieved \nbandwidth, MB/s', fontsize=text_size_big)
    ax.set_xlabel('Compressed data size, MB', fontsize=text_size_big)
    ax.grid()
    ax.legend(fontsize=text_size_small, loc='upper left')

    for r in ['png', 'pdf']:
        plot_name_ = f'{plot_name}.{r}'
        fig.tight_layout(pad=2.0)
        plt.savefig(f'{plot_name_}', format=r, bbox_inches="tight")
        print(f"Plot saved in {plot_name_}")

#
# Plot experiments.
#
def plot_figure_1():
    kModeFixed = 0
    kModeDynamic = 1
    kModeHuffmanOnly = 2
    kModeStatic = 3

    plot_exp_1(plot_name + '_1', prepare_data_1(kModeDynamic, ['xml', 'ooffice', 'reymont', 'sao', 'x-ray', 'osdb', 'dickens', 'samba', 'nci']), 'Dynamic DEFLATE', omit_x_labels=True, omit_y2_axis=True)
    plot_exp_1(plot_name + '_1_1', prepare_data_1(kModeFixed, ['xml', 'ooffice', 'reymont', 'sao', 'x-ray', 'osdb', 'dickens', 'samba', 'nci']), 'Fixed DEFLATE', omit_legend=True, omit_y2_axis=True)
    plot_exp_1(plot_name + '_1_2', prepare_data_1(kModeDynamic, ['webster', 'mozilla', 'pythongrpc', 'pillow']), 'Dynamic DEFLATE', omit_x_labels=True, omit_legend=True, omit_y1_axis=True)
    plot_exp_1(plot_name + '_1_3', prepare_data_1(kModeFixed, ['webster', 'mozilla', 'pythongrpc', 'pillow']), 'Fixed DEFLATE', omit_legend=True, omit_y1_axis=True)
    plot_exp_1_a(plot_name + '_1_4', prepare_data_1(kModeDynamic, None), 'Dynamic DEFLATE')
    plot_exp_1_a(plot_name + '_1_5', prepare_data_1(kModeFixed, None), 'Fixed DEFLATE')
    

def plot_figure_2():
    prepare_and_plot_exp_2(plot_name + '_2', ['mozilla', 'pythongrpc'])

def plot_figure_3():
    kParallelFixed = 0
    kParallelDynamic = 1
    kParallelCanned = 2

    prepare_and_plot_exp_3(plot_name + '_3', ['xml', 'mozilla', 'pillow'], [kParallelFixed, kParallelDynamic])

def plot_figure_4():
    prepare_and_plot_exp_4(plot_name + '_4', ['mozilla', 'pillow'])

def plot_figure_5():
    prepare_and_plot_exp_5(plot_name + '_5')

if for_paper:
    plot_figure_1()
    plot_figure_2()
    plot_figure_3()
    plot_figure_4()
    plot_figure_5()
