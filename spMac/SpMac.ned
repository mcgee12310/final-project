package node.communication.mac.spMac;

simple SpMac like node.communication.mac.iMac {
    parameters:
        int macMaxPacketSize = default(0);
        int macPacketOverhead = default(8);
        int macBufferSize = default(32);
        
        double maxBroadcastRange = default(400.0); // Bán kính R
        int maxHopCount = default(20);
        
        // Tham số riêng của Slotted p-Persistence
        int numSlots = default(10);                 // Số lượng khe thời gian (N_s)
        double slotDuration = default(0.01);       // Thời lượng 1 khe (10ms)
        double probabilityP = default(0.5);        // Xác suất phát p khi đến lượt
        
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