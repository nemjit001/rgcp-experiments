import pandas as pd
import plotly.express as px

df = pd.read_csv('./datapoints/tcp_tp.csv')
px.bar(df, x='test_num', y="time_ms").show()

