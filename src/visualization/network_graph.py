import plotly.graph_objects as go
import pandas as pd

class NetworkVisualizer:
    def __init__(self, positions):
        """
        :param positions: Dictionary dạng {node_id: {'x': val, 'y': val}}
        """
        self.positions = positions
        self.node_ids = list(positions.keys())
        self.x_coords = [pos['x'] for pos in positions.values()]
        self.y_coords = [pos['y'] for pos in positions.values()]

    def create_base_figure(self):
        fig = go.Figure()

        # Tính toán giới hạn trục để tránh auto-zoom
        padding = 50 # Khoảng cách lề 50m
        x_min, x_max = min(self.x_coords) - padding, max(self.x_coords) + padding
        y_min, y_max = min(self.y_coords) - padding, max(self.y_coords) + padding

        # Vẽ các Node
        fig.add_trace(go.Scatter(
            x=self.x_coords,
            y=self.y_coords,
            mode='markers+text',
            marker=dict(size=25, color='rgba(100, 149, 237, 0.8)', line=dict(width=2, color='DarkSlateGrey')),
            text=[f"Node {nid}" for nid in self.node_ids],
            textposition="top center",
            name='Nodes'
        ))

        # Cấu hình Layout quan trọng để giữ khung hình
        fig.update_layout(
            xaxis=dict(
                range=[x_min, x_max], # CỐ ĐỊNH TRỤC X
                autorange=False, 
                title="X (m)",
                scaleanchor="y", 
                scaleratio=1
            ),
            yaxis=dict(
                range=[y_min, y_max], # CỐ ĐỊNH TRỤC Y
                autorange=False,
                title="Y (m)"
            ),
            uirevision='constant', # GIỮ TRẠNG THÁI ZOOM CỦA NGƯỜI DÙNG
            showlegend=False,
            plot_bgcolor='white'
        )
        return fig
    
    def add_transmission_effects(self, fig, current_events):
        for _, row in current_events.iterrows():
            event_name = str(row['event']).upper()
            
            # --- XỬ LÝ BLACK-BURST (BB) ---
            if 'SEND_BB' in event_name:
                node_id = int(row['node'])
                x0, y0 = self.positions[node_id]['x'], self.positions[node_id]['y']
                
                # Vẽ 3 vòng tròn đỏ đồng tâm mở rộng để tạo hiệu ứng sóng
                base_r = 10 # Bán kính cơ sở
                steps = [1, 2.5, 4] # Các hệ số mở rộng
                opacities = [0.8, 0.6, 0.4] # Độ mờ giảm dần

                for i in range(3):
                    r = base_r * steps[i]
                    fig.add_shape(type="circle",
                        xref="x", yref="y",
                        x0=x0-r, y0=y0-r, x1=x0+r, y1=y0+r,
                        line=dict(color="red", width=2),
                        fillcolor="red", opacity=opacities[i]
                    )
                continue # Xong BB, chuyển sự kiện tiếp theo

            # --- 2. HIỆU ỨNG SÓNG LAN TỎA BROADCAST (RTB, CTB) ---
            if 'SEND' in event_name:
                src_id = int(row['node'])
                if src_id not in self.positions: continue
                x0, y0 = self.positions[src_id]['x'], self.positions[src_id]['y']
                
                # Xác định loại gói tin và màu sắc
                p_type = str(row.get('type', 'DATA')).upper()
                dest = str(row.get('to', '')).upper()
                color = "gray"
                if 'RTB' in p_type: color = "orange"
                elif 'CTB' in p_type: color = "green"
                elif 'DATA' in p_type or 'SEND_DATA' in event_name: color = "blue"

                # Vẽ Broadcast dạng sóng lan tỏa
                if dest == 'BROADCAST' or 'BROADCAST' in str(row.get('to')):
                    # Vẽ 4 vòng tròn đồng tâm màu cam/magenta mở rộng
                    max_r = 80 # Bán kính tối đa của sóng
                    steps = [0.25, 0.5, 0.75, 1.0] # Các hệ số mở rộng
                    opacities = [0.8, 0.6, 0.4, 0.2] # Độ mờ giảm dần

                    for i in range(4):
                        r = max_r * steps[i]
                        fig.add_shape(type="circle",
                            xref="x", yref="y",
                            x0=x0-r, y0=y0-r, x1=x0+r, y1=y0+r,
                            line=dict(color=color, width=2, dash="dashdot"),
                            opacity=opacities[i]
                        )
                
                # Vẽ Unicast (Mũi tên)
                else:
                    try:
                        dst_id = int(float(row['to']))
                        if dst_id in self.positions:
                            x1, y1 = self.positions[dst_id]['x'], self.positions[dst_id]['y']
                            fig.add_annotation(
                                x=x1, y=y1, ax=x0, ay=y0,
                                xref="x", yref="y", axref="x", ayref="y",
                                showarrow=True, arrowhead=3, arrowsize=1.2,
                                arrowcolor=color, text=p_type, bgcolor="white",
                                font=dict(color=color, size=9)
                            )
                    except: pass

    def update_node_states(self, fig, active_states):
        """
        Cập nhật màu sắc node dựa trên trạng thái (Dùng cho phần động sau này).
        :param active_states: Dict {node_id: 'STATE_NAME'}
        """
        # Màu sắc quy định cho từng State
        color_map = {
            'IDLE': 'lightgray',
            'WAIT_CTB': 'cyan',
            'CONTENDING': 'gold',
            'PRE_CTB': 'orange',
            'WAIT_DATA': 'green'
        }
        
        # Logic cập nhật màu sẽ được thêm vào phần Callback của Dash
        pass
