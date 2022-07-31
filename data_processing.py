import os, re, glob, json
import pandas as pd

OUT_DIR = "./out/"
DATAPOINT_DIR = "./datapoints/"

def timestr_to_ms(string) -> int:
    pattern = r'(\d+)s:(\d+)ms'
    match = re.match(pattern, string)

    if match is None:
        return -1

    sec = int(match.group(1))
    msec = int(match.group(2))

    return (sec * 1000) + msec

def process_tcp_tp(path, output_file) -> bool:
    print("TCP tp...")

    with open(path, "r") as tcp_tp_file:
        lines = tcp_tp_file.readlines()
        header_regex = r"^(\d+)\tTCP THROUGHPUT$"
        data_regex = r"^(Buffers Not Valid|Buffers Valid) @ ((\d+s):(\d+ms))$"

        data_dict = {
            "test_num": [],
            "b_valid": [],
            "time_ms": []
        }

        for i in range(0, len(lines), 2):
            match_header = re.match(header_regex, lines[i])
            match_data = re.match(data_regex, lines[i + 1])

            if match_header is None or match_data is None:
                print("Error: data format is wrong")
                return False

            data_dict["test_num"].append(match_header.group(1))
            data_dict["b_valid"].append((match_data.group(1) == "Buffers Valid"))
            data_dict["time_ms"].append(
                timestr_to_ms(match_data.group(2))
            )

    df = pd.DataFrame.from_dict(data_dict)
    df.to_csv(output_file, index=False)

    return True

def process_rgcp_tp(path, output_file) -> bool:
    print("RGCP tp...")

    rgcp_tp_files = glob.glob(path)
    rgcp_tp_files.sort()

    filename_regex = r".*rgcp_tp_(\d+)_(\d+)"
    data_regex = r"^(Buffers Not Valid|Buffers Valid) @ ((\d+s):(\d+ms))$"

    data_dict = {
        "peer_count": [],
        "test_num": [],
        "b_valid": [],
        "time_ms": []
    }

    for filename in rgcp_tp_files:
        match = re.match(filename_regex, filename)

        if match is None:
            print("Error: no match found on filename")
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
                print("Error: data format is wrong")
                return False

            data_dict["peer_count"].append(peerCount)
            data_dict["test_num"].append(testNum)
            data_dict["b_valid"].append((data_match.group(1) == "Buffers Valid"))
            data_dict["time_ms"].append(
                timestr_to_ms(data_match.group(2))
            )

    df = pd.DataFrame.from_dict(data_dict)
    df.sort_values(["peer_count", "test_num"], inplace=True)
    df.to_csv(output_file, index=False)

    return True

def process_cpu_load(path, output_file) -> bool:
    print("CPU load...")

    cpu_load_files = glob.glob(path)
    cpu_load_files.sort()

    filename_regex = r'.*rgcp_stab_cpu_(\d+)_(\d+)_(\d+)'

    data_dict = {
        "test_num": [],
        "peer_count": [],
        "runtime_seconds": [],
        "timestamp": [],
        "seconds_from_start": []
    }

    for filename in cpu_load_files:
        match = re.match(filename_regex, filename)

        if match is None:
            print("Error: no match found on filename")
            return False

        testNum = int(match.group(1))
        peerCount = int(match.group(2))
        runtime = int(match.group(3))

        with open(filename, "r") as json_file:
            cpu_load = json.load(json_file)
            for host in cpu_load["sysstat"]["hosts"]:
                for idx, statistic in enumerate(host["statistics"]):
                    for load in statistic['cpu-load']:
                        data_dict["test_num"].append(testNum)
                        data_dict["peer_count"].append(peerCount)
                        data_dict["runtime_seconds"].append(runtime)
                        data_dict["seconds_from_start"].append(idx)
                        data_dict["timestamp"].append(statistic["timestamp"])

                        for key in load.keys():
                            try:
                                data_dict[key].append(load[key])
                            except KeyError:
                                data_dict.setdefault(key, [load[key]])

    df = pd.DataFrame(data_dict)
    df.sort_values(["test_num", "peer_count", "runtime_seconds"], inplace=True)
    df.to_csv(output_file, index=False)

    return True

def process_mem_load(path, output_file) -> bool:
    print("Mem load...")

    mem_load_files = glob.glob(path)
    mem_load_files.sort()

    filename_regex = r'.*rgcp_stab_ram_(\d+)_(\d+)_(\d+)'
    header_regex = r'^\s+(total)\s+(used)\s+(free)\s+(shared)\s+(buffers)\s+(cache)\s+(available).*$'
    totals_regex = r'^(Total):\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)$'
    swap_regex =  r'^(Swap):\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)$'
    mem_regex = r'^(Mem):\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)\s+(\d+|\d+,\d+)$'

    data_dict = {
        "test_num": [],
        "peer_count": [],
        "runtime_seconds": [],
        "timestamp": [],
        "source": []
    }

    for filename in mem_load_files:
        match = re.match(filename_regex, filename)

        if match is None:
            print("Error: no match found on filename")
            return False

        testNum = int(match.group(1))
        peerCount = int(match.group(2))
        runtime = int(match.group(3))

        with open(filename, "r") as file:
            lines = file.readlines()
            seconds = 0
            
            for line in lines:
                line = line.strip("\n\r")
                header_match = re.match(header_regex, line)
                totals_match = re.match(totals_regex, line)
                swap_match = re.match(swap_regex, line)
                mem_match = re.match(mem_regex, line)

                if line == "":
                    seconds += 1
                    continue
                elif header_match is not None:
                    for header in header_match.groups():
                        if header not in data_dict.keys():
                            data_dict.setdefault(header, [])
                    
                    continue
                elif totals_match is not None:
                    data_dict["source"].append(totals_match.group(1))
                    data_dict["total"].append(int(totals_match.group(2)))
                    data_dict["used"].append(int(totals_match.group(3)))
                    data_dict["free"].append(int(totals_match.group(4)))
                    data_dict["shared"].append(None)
                    data_dict["buffers"].append(None)
                    data_dict["cache"].append(None)
                    data_dict["available"].append(None)

                elif swap_match is not None:
                    data_dict["source"].append(swap_match.group(1))
                    data_dict["total"].append(int(swap_match.group(2)))
                    data_dict["used"].append(int(swap_match.group(3)))
                    data_dict["free"].append(int(swap_match.group(4)))
                    data_dict["shared"].append(None)
                    data_dict["buffers"].append(None)
                    data_dict["cache"].append(None)
                    data_dict["available"].append(None)
                elif mem_match is not None:
                    data_dict["source"].append(mem_match.group(1))
                    data_dict["total"].append(int(mem_match.group(2)))
                    data_dict["used"].append(int(mem_match.group(3)))
                    data_dict["free"].append(int(mem_match.group(4)))
                    data_dict["shared"].append(int(mem_match.group(5)))
                    data_dict["buffers"].append(int(mem_match.group(6)))
                    data_dict["cache"].append(int(mem_match.group(7)))
                    data_dict["available"].append(int(mem_match.group(8)))
                else:
                    print("Error: output file format is wrong")
                    return False
                
                data_dict["test_num"].append(testNum)
                data_dict["peer_count"].append(peerCount)
                data_dict["runtime_seconds"].append(runtime)
                data_dict["timestamp"].append(seconds)

    df = pd.DataFrame(data_dict)
    df.sort_values(["test_num", "peer_count", "runtime_seconds", "source", "timestamp"], inplace=True)
    df.to_csv(output_file, index=False)

    return True

def do_throughput_experiments() -> None:
    print("Processing `throughput` experiment:")

    if not process_tcp_tp(OUT_DIR + "tcp_tp", DATAPOINT_DIR + "tcp_tp.csv"):
        print("\tTCP TP data processing failed")
        return

    if not process_rgcp_tp(OUT_DIR + "rgcp_tp_*", DATAPOINT_DIR + 'rgcp_tp.csv'):
        print("\tRGCP TP data processing failed")
        return

    print("\tOK")

def do_stability_experiments() -> None:
    print("Processing `stability` experiment:")

    if not process_cpu_load(OUT_DIR + "rgcp_stab_cpu_*", DATAPOINT_DIR + 'stab_cpu_load.csv'):
        print("CPU load data processing failed")
        return

    if not process_mem_load(OUT_DIR + "rgcp_stab_ram_*", DATAPOINT_DIR + 'stab_mem_load.csv'):
        print("Memory load data processing failed")
        return
    
    print("\tOK")

def do_simul_test_experiments() -> None:
    print("Processing `throughput during stability` experiment:")
    
    print("\tNYI")

if __name__ == "__main__":
    if not os.path.exists(DATAPOINT_DIR):
        print("Created datapoint directory")
        os.mkdir(DATAPOINT_DIR)

    do_throughput_experiments()
    do_stability_experiments()
    do_simul_test_experiments()
    
