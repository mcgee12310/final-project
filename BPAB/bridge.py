# -*- coding: utf-8 -*-

import traci
import socket
import time
import sys
import math

# ==========================================
# CAU HINH HE THONG
# ==========================================
SUMO_CONFIG_FILE = "highway.sumocfg"
NET_FILE = "highway.net.xml"
CASTALIA_IP = "127.0.0.1"
CASTALIA_PORT = 9999
STEP_LENGTH = 1
PLAYBACK_SPEED = 2.0
INTERSECTION_THRESHOLD = 30.0

def load_intersections(net_file):
    """Doc toa do cac giao lo tu file .net.xml bang cach doc XML truc tiep"""
    try:
        import xml.etree.ElementTree as ET
        tree = ET.parse(net_file)
        root = tree.getroot()
        coords = []
        for junction in root.findall('junction'):
            jid = junction.get('id', '')
            jtype = junction.get('type', '')
            # Bo qua junction noi bo va dead-end
            if jid.startswith(':'):
                continue
            if jtype in ('internal', 'dead_end'):
                continue
            try:
                x = float(junction.get('x', '0'))
                y = float(junction.get('y', '0'))
                coords.append((x, y))
                print("  Giao lo [%s] tai (%.1f, %.1f)" % (jid, x, y))
            except ValueError:
                continue
        print("Da tai xong %d giao lo tu map." % len(coords))
        return coords
    except Exception, e:
        print("Loi doc file map: %s" % str(e))
        return []

def is_near_intersection(x, y, intersections, threshold):
    """Kiem tra xe co gan giao lo nao khong"""
    for (ix, iy) in intersections:
        dist = math.sqrt((x - ix)**2 + (y - iy)**2)
        if dist < threshold:
            return True
    return False

def is_inside_intersection_by_road(vid):
    """
    Kiem tra xe co dang nam trong junction internal lane hay khong
    SUMO quy uoc lane/edge noi bo giao lo bat dau bang ':'
    """
    try:
        road_id = traci.vehicle.getRoadID(vid)

        # Xe dang o internal edge cua junction
        if road_id.startswith(":"):
            return True

        return False

    except Exception:
        return False

def main():
    # 1. Tai du lieu giao lo truoc khi chay
    intersections = load_intersections(NET_FILE)

    # 2. KET NOI VOI CASTALIA
    # Python 2.6: dung try/except rieng, khong dung "except X as e" cho 2.5 compat
    try:
        castalia_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        castalia_sock.connect((CASTALIA_IP, CASTALIA_PORT))
    except socket.error:
        print("Khong ket noi duoc Castalia!")
        sys.exit(1)

    # 3. KHOI DONG SUMO
    sumoCmd = ["sumo", "-c", SUMO_CONFIG_FILE, "--step-length", str(STEP_LENGTH)]
    try:
        traci.start(sumoCmd)
    except Exception, e:                      # Python 2.6 syntax (khong dung "as")
        print("Loi khoi dong SUMO: %s" % str(e))
        castalia_sock.close()
        sys.exit(1)

    # 4. VONG LAP DONG BO
    try:
        while traci.simulation.getMinExpectedNumber() > 0:
            traci.simulationStep()
            current_sumo_time = traci.simulation.getTime()

            vehicle_ids = traci.vehicle.getIDList()
            for vid in vehicle_ids:
                x_sumo, y_sumo = traci.vehicle.getPosition(vid)

                # Python 2.6: filter() tra ve list, khong phai iterator -> dung truc tiep
                digits = filter(str.isdigit, vid)
                node_id = int(''.join(digits))

                # is_near = is_near_intersection(x_sumo, y_sumo,
                #                                intersections,
                #                                INTERSECTION_THRESHOLD)
                # is_inter = 1 if is_near else 0

                is_inter = 1 if is_inside_intersection_by_road(vid) else 0

                # Python 2.6: dung % formatting thay vi f-string
                cmd = "SET_POS|TIME:%.2f|NODE:%d|X:%.2f|Y:%.2f|INTER:%d\n" % (
                    current_sumo_time, node_id, x_sumo, y_sumo, is_inter
                )

                if is_inter == 1:
                    print("[%.1f] Node %d gan giao lo" % (current_sumo_time, node_id))

                castalia_sock.sendall(cmd)     # Python 2.6: str, khong can .encode()

            time.sleep(STEP_LENGTH / float(PLAYBACK_SPEED))

    except KeyboardInterrupt:
        pass

    # Python 2.6: finally phai tach khoi try/except, dung try/finally rieng
    # hoac viet lai thanh try/except/finally (hop le tu 2.5+)
    finally:
        try:
            traci.close()
        except Exception:
            pass
        try:
            castalia_sock.close()
        except Exception:
            pass

if __name__ == "__main__":
    main()