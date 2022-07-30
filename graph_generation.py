import pandas as pd
import plotly.express as px
import plotly.graph_objects as go

DATAPOINT_DIR = './datapoints/'
OUTPUT_DIR = './media/'

def get_col_avg(df: pd.DataFrame, column_name: str):
    count = len(df[column_name].to_list())
    return df[column_name].sum() / count

def make_tp_graphs(datapoint_dir: str, output_dir: str):
    val_to_string = lambda val : str(val)

    tcp_tp_df = pd.read_csv(datapoint_dir + 'tcp_tp.csv')
    tcp_tp_df['test_num'] = tcp_tp_df["test_num"].map(val_to_string)
    tcp_tp_fig = px.histogram(tcp_tp_df, x='test_num', y="time_ms", barmode="group")
    tcp_avg_rtt = get_col_avg(tcp_tp_df, 'time_ms')

    tcp_tp_fig.add_shape(
        type='line',
        x0=0.0,
        x1=1.0,
        y0=tcp_avg_rtt,
        y1=tcp_avg_rtt,
        line= { "color": 'Red' },
        xref='paper',
        yref='y'
    )

    tcp_tp_fig.show()

make_tp_graphs(DATAPOINT_DIR, OUTPUT_DIR)

