// WppMac.ned
package node.communication.mac.wppMac;

simple WppMac like node.communication.mac.iMac {
    parameters:
        // Các tham số mặc định của Castalia VirtualMac
        int macMaxPacketSize = default(0);
        int macPacketOverhead = default(8);
        int macBufferSize = default(32);
        
        // Các tham số riêng của thuật toán WpP
        double maxBroadcastRange = default(400.0); // Bán kính R
        double jitterMax = default(0.01);          // Trễ ngẫu nhiên tối đa (giây)
        int maxHopCount = default(20);             // Giới hạn số hop (TTL)
        
        // --- Cac tham so ke thua tu VirtualMac ---
        bool collectTraceInfo = default(true); // Bat/Tat ghi log (trace) de debug.
        //debug parameters
    	bool printStateTransitions = default (false);

        // Thoi gian chuyen doi trang thai Radio (Radio delay)
        double phyDelayForRX = default(0.0001); 

    gates:
        output toNetworkModule;
        output toRadioModule;
        input fromNetworkModule;
        input fromRadioModule;
        input fromCommModuleResourceMgr;
}