//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

cplusplus {{
    #include "MacPacket_m.h"
}}

class MacPacket;

enum BPABMessageType {
    BPAB_RTB = 1;
    BPAB_BLACK_BURST = 2;
    BPAB_CTB = 3;
    BPAB_DATA = 4;
    BPAB_CTB_RSU = 5;
    BPAB_RSU_ACK = 6;
};

enum direction {
    EAST = 1;
    WEST = 2;
    NORTH = 3;
    SOUTH = 4;
    INTER = 5;
};

packet BPABPacket extends MacPacket {
    
    int bpabType enum(BPABMessageType);
    int incomingDir enum(direction);
    int direction enum(direction);
	double rtbSentTime;
	
    int sourceId;
    int destinationId;
    double sourceX;
    double sourceY;

    int iteration = 0;      
    int maxIterations = 4;   // N
    double rangeR = 400.0;   // R
    
    double limitL;
    double limitU;
    
    string payload;
}