package node.communication.mac.bpabMac;

// Ke thua tu VirtualMac de co cac cong (gate) mac dinh cua Castalia
simple BpabMac like node.communication.mac.iMac {
    parameters:
        @display("i=block/layer"); // Bieu tuong hien thi trong giao dien OMNeT++

        // --- Cac tham so dac thu cua giao thuc BPAB ---
        int nodeType = default(0);  // 1 = RSU, 0 = Vehicle
        int maxIterations = default(4);        // So vong lap nhi phan (N). N cang lon, vung chon cang hep.
        double rangeR = default(400.0);        // Pham vi truyen dan toi da (R) tinh bang met.
        double slotDuration = default(0.0005);   // Do dai moi khe thoi gian (giay) de phat/nghe Xung den.
        int maxRetries = default(3);
        
        // --- Cau hinh vat ly cho Xung den (Black Burst) ---
        
        double bbTxPower = default(0.0);       // Cong suat phat Xung den (dBm). Nen de o muc cao nhat.
        double rssiThreshold = default(-85.0); // Nguong nang luong (dBm) de xac dinh co Xung den hay khong.

        // --- Cac tham so ke thua tu VirtualMac ---
        bool collectTraceInfo = default(true); // Bat/Tat ghi log (trace) de debug.
        int macBufferSize = default(32);       // So luong goi tin toi da trong hang doi MAC.
        //debug parameters
    	bool printStateTransitions = default (false);
    
    	//mac layer parameters
    	int macMaxPacketSize = default (142);	//128 (Maximum payload for 802.15.4) + 14 (Data packet overhead)
    	int macPacketOverhead = default (14);

        // Thoi gian chuyen doi trang thai Radio (Radio delay)
        double phyDelayForRX = default(0.0001); 
        
        gates: 
 	output toNetworkModule;
	output toRadioModule;
	input fromNetworkModule;
	input fromRadioModule;
	input fromCommModuleResourceMgr;
}