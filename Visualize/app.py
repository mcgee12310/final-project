import os

import dash
from dash import Input, Output, State, dcc, html
import plotly.graph_objects as go

from src.parser.log_parser import CastaliaParser
from src.visualization.network_graph import NetworkVisualizer

LOG_FILE_PATH = "data/raw/test01.txt"

app = dash.Dash(__name__, title="Castalia BPAB Visualizer")


def load_data():
    if not os.path.exists(LOG_FILE_PATH):
        return None, {}
    parser = CastaliaParser(LOG_FILE_PATH)
    df = parser.parse()
    positions = parser.get_node_positions(df)
    return df, positions


df_log, node_positions = load_data()

_time_min = df_log["time"].min() if df_log is not None else 0
_time_max = df_log["time"].max() if df_log is not None else 10

app.layout = html.Div(
    style={"fontFamily": "Arial, sans-serif", "padding": "20px"},
    children=[
        dcc.Interval(
            id="auto-play-interval",
            interval=200,
            n_intervals=0,
            disabled=True,
        ),

        # Header
        html.Div(
            style={"padding": "10px", "backgroundColor": "#f8f9fa", "marginBottom": "20px"},
            children=[
                html.H1(
                    "Castalia BPAB Protocol Visualizer",
                    style={"textAlign": "center", "color": "#2c3e50"},
                ),
                html.P(
                    "Công cụ trực quan hóa quá trình truyền tin và tranh chấp nút mạng",
                    style={"textAlign": "center", "color": "#7f8c8d"},
                ),
            ],
        ),

        # Main content: network graph + event log
        html.Div(
            style={"display": "flex"},
            children=[
                html.Div(
                    style={"width": "75%", "verticalAlign": "top"},
                    children=[dcc.Graph(id="network-graph", style={"height": "75vh"})],
                ),
                html.Div(
                    style={"width": "23%", "marginLeft": "2%"},
                    children=[
                        html.H4("Sự kiện tại thời điểm chọn:"),
                        html.Div(
                            id="event-details",
                            style={
                                "height": "65vh",
                                "overflowY": "scroll",
                                "border": "1px solid #ddd",
                                "padding": "10px",
                                "fontSize": "12px",
                                "backgroundColor": "#fdfdfd",
                            },
                        ),
                    ],
                ),
            ],
        ),

        # Control bar
        html.Div(
            style={
                "padding": "30px",
                "backgroundColor": "#f8f9fa",
                "marginTop": "20px",
                "borderRadius": "10px",
            },
            children=[
                html.Div(
                    style={"marginBottom": "10px"},
                    children=[
                        html.B("Dòng thời gian (Simulation Time): "),
                        html.Span(
                            id="current-time-display",
                            style={"color": "#e74c3c", "fontWeight": "bold"},
                        ),
                    ],
                ),
                dcc.Slider(
                    id="time-slider",
                    min=_time_min,
                    max=_time_max,
                    step=0.01,
                    value=_time_min,
                    marks={i: f"{i}s" for i in range(0, int(_time_max + 2), 5)},
                    updatemode="drag",
                ),
                html.Div(
                    style={"marginTop": "20px", "textAlign": "center"},
                    children=[
                        html.Button("Play", id="play-button", n_clicks=0, style={"marginRight": "10px"}),
                        html.Button("Refresh Data", id="refresh-button", n_clicks=0),
                    ],
                ),
            ],
        ),
    ],
)


@app.callback(
    Output("network-graph", "figure"),
    Output("current-time-display", "children"),
    Output("event-details", "children"),
    Input("time-slider", "value"),
)
def update_visuals(selected_time):
    if selected_time is None or df_log is None:
        return go.Figure(), "0.0000 s", [html.P("Đang tải dữ liệu...")]

    viz = NetworkVisualizer(node_positions)
    fig = viz.create_base_figure()

    window = 0.5
    current_events = df_log[
        (df_log["time"] <= selected_time) & (df_log["time"] > selected_time - window)
    ]

    viz.add_transmission_effects(fig, current_events)

    event_list = [
        html.P(f"[{row['time']:.4f}] Node {row['node']}: {row['event']} {row.get('type', '')}")
        for _, row in current_events.iterrows()
    ]

    return fig, f"{selected_time:.4f} s", event_list


@app.callback(
    Output("auto-play-interval", "disabled"),
    Output("play-button", "children"),
    Output("play-button", "style"),
    Input("play-button", "n_clicks"),
)
def toggle_play(n_clicks):
    if n_clicks % 2 == 1:
        return False, "Pause", {"backgroundColor": "#e74c3c", "color": "white"}
    return True, "Play", {"backgroundColor": "#2ecc71", "color": "white"}


@app.callback(
    Output("time-slider", "value"),
    Input("auto-play-interval", "n_intervals"),
    State("time-slider", "value"),
    State("play-button", "n_clicks"),
    prevent_initial_call=True,
)
def advance_slider(n_intervals, current_value, play_clicks):
    if play_clicks % 2 == 1:
        return min(current_value + 0.01, _time_max)
    return current_value


if __name__ == "__main__":
    os.makedirs("data/raw", exist_ok=True)
    print("Ứng dụng đang khởi chạy tại http://127.0.0.1:8050")
    app.run(debug=True, port=8050)
