package node.communication.mac.floodingMac;

// Ke thua tu VirtualMac de co cac cong (gate) mac dinh cua Castalia
simple FloodingMac like node.communication.mac.iMac {

    parameters:

        @display("i=block/layer");

        // =========================================================
        // Flooding parameters
        // =========================================================

        // Delay ngau nhien truoc khi rebroadcast
        // Giup giam broadcast storm dong thoi
        double jitterMax = default(0.05);

        // So hop toi da cho phep
        int maxHopCount = default(20);

        // Bat/tat rebroadcast packet trung lap
        bool enableDuplicateDetection = default(true);

        // =========================================================
        // Radio / PHY parameters
        // =========================================================

        // Cong suat phat
        double txPower = default(0.0);

        // RSSI toi thieu de chap nhan packet
        double rssiThreshold = default(-85.0);

        // =========================================================
        // VirtualMac inherited parameters
        // =========================================================

        bool collectTraceInfo = default(true);

        int macBufferSize = default(32);

        // Debug
        bool printStateTransitions = default(false);

        // MAC packet size
        int macMaxPacketSize = default(142);

        int macPacketOverhead = default(14);

        // Radio switching delay
        double phyDelayForRX = default(0.0001);

    gates:
	output toNetworkModule;
	output toRadioModule;
	input fromNetworkModule;
	input fromRadioModule;
	input fromCommModuleResourceMgr;

}