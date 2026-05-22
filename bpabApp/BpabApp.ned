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

package node.application.bpabApp;

simple BpabApp like node.application.iApplication {
    parameters:
        string applicationID = default("bpabApp");
        bool collectTraceInfo = default (true);
        int priority = default (1);
        int packetHeaderOverhead = default (8);	// in bytes
		int constantDataPayload = default (12);	// in bytes
		
        // --- Các tham số tự định nghĩa ---
        bool isSender = default(false);
        double sendInterval @unit(s) = default(1s); // Tần suất gửi gói tin
        
        @class(BpabApp);
        
	gates:
     	output toCommunicationModule;
    	output toSensorDeviceManager;
    	input fromCommunicationModule;
    	input fromSensorDeviceManager;
    	input fromResourceManager;
}