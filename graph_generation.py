import os
import pandas as pd
import plotly.express as px

DATAPOINT_DIR = './datapoints/'
OUTPUT_DIR = './graphs/'

def get_col_avg(df: pd.DataFrame, column_name: str):
    count = len(df[column_name].to_list())
    return df[column_name].sum() / count

def make_tcp_tp_graph(datapoint_dir: str):
    tp_df = pd.read_csv(datapoint_dir + 'tcp_tp.csv')

    avg_rtt = get_col_avg(tp_df, 'time_ms')

    tp_fig = px.histogram(tp_df, x='test_num', y="time_ms", barmode="group")
    tp_fig.update_xaxes(type='category')

    tp_fig.add_shape(
        type='line',
        x0=0.0,
        x1=1.0,
        y0=avg_rtt,
        y1=avg_rtt,
        line= { "color": 'Red' },
        xref='paper',
        yref='y'
    )

    return tp_fig

def make_rgcp_simple_tp_graph(datapoint_dir: str):
    tp_df = pd.read_csv(datapoint_dir + 'rgcp_tp.csv')
    tp_df = tp_df[tp_df['peer_count'] == 2]

    if len(tp_df.index) == 0:
        print("Warning: empty dataframe for simple rgcp tp graph")

    avg_rtt = get_col_avg(tp_df, 'time_ms')

    tp_fig = px.histogram(tp_df, x='test_num', y="time_ms", barmode="group")
    tp_fig.update_xaxes(type='category')

    tp_fig.add_shape(
        type='line',
        x0=0.0,
        x1=1.0,
        y0=avg_rtt,
        y1=avg_rtt,
        line= { "color": 'Red' },
        xref='paper',
        yref='y'
    )

    return tp_fig

def make_rgcp_avg_tp_graph(datapoint_dir: str):
    rgcp_tp_df = pd.read_csv(datapoint_dir + 'rgcp_tp.csv')
    
    rgcp_tp_df_list = { count: rgcp_tp_df[rgcp_tp_df['peer_count'] == count] for count in rgcp_tp_df['peer_count'].unique() }
    rgcp_avg_rtt = {
        "peer_count": [ 0 ],
        "avg_time_ms": [ 0 ]
    }

    for key in rgcp_tp_df_list:
        if key == 2:
            continue

        rgcp_avg_rtt['peer_count'].append(key)
        rgcp_avg_rtt['avg_time_ms'].append(get_col_avg(rgcp_tp_df_list[key], 'time_ms'))

    df = pd.DataFrame(rgcp_avg_rtt)
    tp_fig = px.line(df, x='peer_count', y='avg_time_ms')
    tp_fig.update_xaxes(type='category', rangemode='tozero')
    tp_fig.update_yaxes(rangemode='tozero')

    return tp_fig

def make_rgcp_stab_avg_tp_graph(datapoint_dir: str):
    rgcp_tp_df = pd.read_csv(datapoint_dir + 'rgcp_tp_during_stab.csv')
    baseline_df = pd.read_csv(datapoint_dir + 'rgcp_tp.csv')

    baseline_tp_df_list = { count: baseline_df[baseline_df['peer_count'] == count] for count in baseline_df['peer_count'].unique() }
    rgcp_tp_df_list = { count: rgcp_tp_df[rgcp_tp_df['peer_count'] == count] for count in rgcp_tp_df['peer_count'].unique() }

    avg_rtt = {
        "test": [ 'No Stability', 'During Stability' ],
        "peer_count": [ 0, 0 ],
        "avg_time_ms": [ 0, 0 ]
    }

    for key in baseline_tp_df_list:
        if key == 2:
            continue
        
        avg_rtt['test'].append('No Stability')
        avg_rtt['peer_count'].append(key)
        avg_rtt['avg_time_ms'].append(get_col_avg(baseline_tp_df_list[key], 'time_ms'))

    for key in rgcp_tp_df_list:
        if key == 2:
            continue

        avg_rtt['test'].append('During Stability')
        avg_rtt['peer_count'].append(key)
        avg_rtt['avg_time_ms'].append(get_col_avg(rgcp_tp_df_list[key], 'time_ms'))

    df = pd.DataFrame(avg_rtt)

    tp_fig = px.line(
        df, 
        x='peer_count', 
        y='avg_time_ms', 
        color='test',
        line_dash='test', 
        line_dash_map={
            'No Stability': 'dot',
            'During Stability': 'solid'
        }
    )

    tp_fig.update_xaxes(type='category', rangemode='tozero')
    tp_fig.update_yaxes(rangemode='tozero')

    return tp_fig

def make_tp_graphs(datapoint_dir: str) -> dict:
    return {
        "tcp_tp": make_tcp_tp_graph(datapoint_dir),
        "rgcp_simple": make_rgcp_simple_tp_graph(datapoint_dir),
        "rgcp_avg": make_rgcp_avg_tp_graph(datapoint_dir),
        "rgcp_avg_during_stab": make_rgcp_stab_avg_tp_graph(datapoint_dir)
    }

def make_cpu_util_graph(datapoint_dir: str, col_name: str):
    util_df = pd.read_csv(datapoint_dir + 'stab_cpu_load.csv')
    util_df = util_df[util_df['cpu'] == "all"]

    dfs = [ util_df[util_df['peer_count'] == count] for count in util_df['peer_count'].unique() ]
    out_df = None

    for df in dfs:
        _dfs = [ df[df['test_num'] == num] for num in util_df['test_num'].unique() ]

        total_df = _dfs[0][['test_num', 'peer_count', 'seconds_from_start', col_name]].copy(deep=True)
        total_df.reset_index(inplace=True)

        for i in range(1, len(_dfs)):
            _df = _dfs[i].copy(deep=True)
            _df.reset_index(inplace=True)
            total_df[col_name] = total_df[col_name] + _df[col_name]

        total_df[col_name] = total_df[col_name] / len(_dfs)

        if out_df is None:
            out_df = total_df
        else:
            out_df = pd.concat([out_df, total_df])

    util_fig = px.line(
        out_df,
        x='seconds_from_start',
        y=col_name,
        color='peer_count'
    )
    
    return util_fig

def make_mem_util_graph(datapoint_dir: str):
    util_df = pd.read_csv(datapoint_dir + 'stab_mem_load.csv')
    util_df = util_df[util_df['source'] == "Total"]

    dfs = [ util_df[util_df['peer_count'] == count] for count in util_df['peer_count'].unique() ]
    out_df = None

    for df in dfs:
        _dfs = [ df[df['test_num'] == num] for num in util_df['test_num'].unique() ]

        total_df = _dfs[0][['test_num', 'peer_count', 'timestamp', 'used']].copy(deep=True)
        total_df.reset_index(inplace=True)

        for i in range(1, len(_dfs)):
            _df = _dfs[i].copy(deep=True)
            _df.reset_index(inplace=True)
            total_df['used'] = total_df['used'] + _df['used']

        total_df['used'] = total_df['used'] / len(_dfs)

        if out_df is None:
            out_df = total_df
        else:
            out_df = pd.concat([out_df, total_df])

    util_fig = px.line(
        out_df,
        x='timestamp',
        y='used',
        color='peer_count'
    )
    
    return util_fig

def make_stab_graphs(datapoint_dir: str) -> dict:
    return {
        "usr_cpu_util_per_second_over_peers": make_cpu_util_graph(datapoint_dir, 'usr'),
        "sys_cpu_util_per_second_over_peers": make_cpu_util_graph(datapoint_dir, 'sys'),
        "mem_util_per_second_over_peers": make_mem_util_graph(datapoint_dir)
    }

if __name__ == '__main__':
    tp_graphs = make_tp_graphs(DATAPOINT_DIR)
    stab_graphs = make_stab_graphs(DATAPOINT_DIR)

    if not os.path.exists(OUTPUT_DIR):
        os.mkdir(OUTPUT_DIR)

    for graph_name in tp_graphs:
        print(f"<graph @ {id(tp_graphs[graph_name])}>")
        tp_graphs[graph_name].write_image(OUTPUT_DIR + graph_name + '.png')

    for graph_name in stab_graphs:
        print(f"<graph @ {id(graph_name)}>")
        stab_graphs[graph_name].write_image(OUTPUT_DIR + graph_name + '.png')
