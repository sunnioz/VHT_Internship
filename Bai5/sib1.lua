--[[
struct SIB1
{
    short s_PF_OFFSET;      /*Phan bu PF*/
    short s_DRX_CYCLE;      /*Chu ki DRX*/
    short s_N;              /*So lan gNB gui ban tin RRC_Paging tren 1 chu ki DRX*/
};

]]

-- Create a new protocol named SIB1
local sib_proto = Proto("SIB1", "SIB1 Message")

-- Define the fields for the SIB1 protocol
local fields = sib_proto.fields
fields.s_pf_offset = ProtoField.int16("sib1.s_pf_offset", "PF Offset", base.DEC)
fields.s_drx_cycle = ProtoField.int16("sib1.s_drx_cycle", "DRX Cycle", base.DEC)
fields.s_n = ProtoField.int16("sib1.s_n", "N", base.DEC)

-- Dissector function
function sib_proto.dissector(buffer, pinfo, tree)
    -- Set the protocol column in the UI
    pinfo.cols.protocol = "SIB1"

    -- Create the protocol tree
    local subtree = tree:add(sib_proto, buffer(), "SIB1 Message")

    -- Check if the buffer is large enough to contain the entire message
    if buffer:len() < 6 then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR, "SIB1 message length is too short")
        return
    end

    -- Add fields to the tree
    subtree:add_le(fields.s_pf_offset, buffer(0, 2))
    subtree:add_le(fields.s_drx_cycle, buffer(2, 2))
    subtree:add_le(fields.s_n, buffer(4, 2))
end





-- Register the dissector to UDP port 5000
local udp_port = DissectorTable.get("udp.port")
udp_port:add(5000, sib_proto)
