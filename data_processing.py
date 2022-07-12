import os, re, glob
from numpy import mat
import pandas as pd

OUT_DIR = "./out/"
DATAPOINT_DIR = "./datapoints/"

RGCP_TP_PATH_STR = OUT_DIR + "rgcp_tp_*"
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

if __name__ == "__main__":
    if not os.path.exists(DATAPOINT_DIR):
        print("Created datapoint directory")
        os.mkdir(DATAPOINT_DIR)

    if not process_tcp_tp(TCP_TP_PATH, DATAPOINT_DIR):
        print("TCP TP data processing failed")

    if not process_rgcp_tp(RGCP_TP_PATH_STR, DATAPOINT_DIR + 'rgcp_tp.csv'):
        print("RGCP TP data processing failed")
