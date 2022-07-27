import os, re, glob, json
import pandas as pd

OUT_DIR = "./out/"
DATAPOINT_DIR = "./datapoints/"

RGCP_TP_PATH_STR = OUT_DIR + "rgcp_tp_*"
RGCP_CPU_PATH_STR = OUT_DIR + "rgcp_stab_cpu_*"
RGCP_MEM_PATH_STR = OUT_DIR + "rgcp_stab_ram_*"
TCP_TP_PATH = OUT_DIR + "tcp_tp"

def process_tcp_tp(path, output_dir) -> bool:
    with open(path, "r") as tcp_tp_file:
        lines = tcp_tp_file.readlines()
        header_regex = r"^(\d+)\tTCP THROUGHPUT$"
        data_regex = r"^(Buffers Not Valid|Buffers Valid) @ ((\d+s):(\d+ms))$"

        data_dict = {
            "testNum": [],
            "bValid": [],
            "time": []
        }

        for i in range(0, len(lines), 2):
            match_header = re.match(header_regex, lines[i])
            match_data = re.match(data_regex, lines[i + 1])

            if match_header is None or match_data is None:
                return False

            data_dict["testNum"].append(match_header.group(1))
            data_dict["bValid"].append((match_data.group(1) == "Buffers Valid"))
            data_dict["time"].append(match_data.group(2))

    df = pd.DataFrame.from_dict(data_dict)
    df.to_csv(output_dir + 'tcp_tp.csv', index=False)

    return True

def process_rgcp_tp(path, output_file) -> bool:
    rgcp_tp_files = glob.glob(path)
    rgcp_tp_files.sort()

    filename_regex = r".*rgcp_tp_(\d+)_(\d+)"
    data_regex = r"^(Buffers Not Valid|Buffers Valid) @ ((\d+s):(\d+ms))$"

    data_dict = {
        "peerCount": [],
        "testNum": [],
        "bValid": [],
        "time": []
    }

    for filename in rgcp_tp_files:
        match = re.match(filename_regex, filename)

        if match is None:
            return False

        testNum = int(match.group(1))
        peerCount = int(match.group(2))

        with open(filename, "r") as rgcp_tp_file:
            data_lines = rgcp_tp_file.readlines()

            if len(data_lines) == 0:
                continue

            data_line = data_lines[-1]
            data_match = re.match(data_regex, data_line)
            
            if data_match is None:
                return False

            data_dict["peerCount"].append(peerCount)
            data_dict["testNum"].append(testNum)
            data_dict["bValid"].append((data_match.group(1) == "Buffers Valid"))
            data_dict["time"].append(data_match.group(2))

    df = pd.DataFrame.from_dict(data_dict)
    df.sort_values(["peerCount", "testNum"], inplace=True)
    df.to_csv(output_file, index=False)

    return True

def process_cpu_load(path, output_file) -> bool:
    cpu_load_files = glob.glob(path)
    cpu_load_files.sort()

    filename_regex = r'.*rgcp_stab_cpu_(\d+)_(\d+)_(\d+)'

    data_dict = {
        "testNum": [],
        "peerCount": [],
        "runtime": [],
        "timestamp": []
    }

    for filename in cpu_load_files:
        match = re.match(filename_regex, filename)

        if match is None:
            return False

        testNum = int(match.group(1))
        peerCount = int(match.group(2))
        runtime = int(match.group(3))

        with open(filename, "r") as json_file:
            cpu_load = json.load(json_file)
            for host in cpu_load["sysstat"]["hosts"]:
                for statistic in host["statistics"]:
                    for load in statistic['cpu-load']:
                        data_dict["testNum"].append(testNum)
                        data_dict["peerCount"].append(peerCount)
                        data_dict["runtime"].append(runtime)
                        data_dict["timestamp"].append(statistic['timestamp'])

                        for key in load.keys():
                            try:
                                data_dict[key].append(load[key])
                            except KeyError:
                                data_dict.setdefault(key, [load[key]])

    df = pd.DataFrame(data_dict)
    df.sort_values(["testNum", "peerCount", "runtime"], inplace=True)
    df.to_csv(output_file, index=False)

    return True

def process_mem_load(path, output_file) -> bool:
    mem_load_files = glob.glob(path)
    mem_load_files.sort()

    filename_regex = r'.*rgcp_stab_ram_(\d+)_(\d+)_(\d+)'

    for filename in mem_load_files:
        match = re.match(filename_regex, filename)

        if match is None:
            return False

        testNum = int(match.group(1))
        peerCount = int(match.group(2))
        runtime = int(match.group(3))

        with open(filename, "r") as file:
            lines = file.readlines()
            
            for line in lines:
                print(line, end='')

    return True

if __name__ == "__main__":
    if not os.path.exists(DATAPOINT_DIR):
        print("Created datapoint directory")
        os.mkdir(DATAPOINT_DIR)

    if not process_tcp_tp(TCP_TP_PATH, DATAPOINT_DIR):
        print("TCP TP data processing failed")

    if not process_rgcp_tp(RGCP_TP_PATH_STR, DATAPOINT_DIR + 'rgcp_tp.csv'):
        print("RGCP TP data processing failed")

    if not process_cpu_load(RGCP_CPU_PATH_STR, DATAPOINT_DIR + 'stab_cpu_load.csv'):
        print("CPU load data processing failed")

    if not process_mem_load(RGCP_MEM_PATH_STR, DATAPOINT_DIR + 'stab_mem_load.csv'):
        print("Memory load data processing failed")
