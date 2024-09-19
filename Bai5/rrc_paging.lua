--[[
struct Paging_Record
{
    int NG_5G_S_TMSI;       /*Dinh danh tam thoi cua UE*/
    int CN_Domain;          /*Kieu tim goi*/
};

struct RRC_Paging_message
{
    int Message_Type;       /*Kieu ban tin*/
    int num_Paging_record;  /*So luong ban tin paging record*/
    struct Paging_Record paging_record[MAXNROFPAGEREC];
};


]]

local global_buffer = 0

-- Create a new protocol named RRC_Paging
local rrc_paging_proto = Proto("RRC_Paging", "RRC Paging Message")

-- Define the fields for the RRC_Paging protocol
local fields = rrc_paging_proto.fields
fields.message_type = ProtoField.int32("rrc_paging.message_type", "Message Type", base.DEC)
fields.num_paging_record = ProtoField.int32("rrc_paging.num_paging_record", "Number of Paging Record", base.DEC)

-- Define the Paging_Record protocol
local paging_record_proto = Proto("Paging_Record", "Paging Record")
local paging_record_fields = paging_record_proto.fields
paging_record_fields.ng_5g_s_tmsi = ProtoField.int32("paging_record.ng_5g_s_tmsi", "NG 5G-S-TMSI", base.DEC)
paging_record_fields.cn_domain = ProtoField.int32("paging_record.cn_domain", "CN Domain", base.DEC)

-- Dissector function
function rrc_paging_proto.dissector(buffer, pinfo, tree)
    global_buffer = buffer
    -- Set the protocol column in the UI
    pinfo.cols.protocol = "RRC Paging"

    -- Create the protocol tree
    local subtree = tree:add(rrc_paging_proto, buffer(), "RRC Paging Message")

    -- Check if the buffer is large enough to contain the entire message
    if buffer:len() < 8 then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR, "RRC Paging message length is too short")
        return
    end

    -- Add fields to the tree
    subtree:add_le(fields.message_type, buffer(0, 4))
    subtree:add_le(fields.num_paging_record, buffer(4, 4))

    -- Check if the buffer is large enough to contain the entire message
    if buffer:len() < 8 + buffer(4, 4):le_uint() * 8 then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR, "RRC Paging message length is too short")
        return
    end

    -- Create the Paging Record tree
    local paging_record_tree = subtree:add(paging_record_proto, buffer(8, buffer(4, 4):le_uint() * 8), "Paging Record")

    -- Add fields to the Paging Record tree
    for i = 0, buffer(4, 4):le_uint() - 1 do
        local paging_record_subtree = paging_record_tree:add(paging_record_proto, buffer(8 + i * 8, 8), "Paging Record " .. i + 1)
        paging_record_subtree:add_le(paging_record_fields.ng_5g_s_tmsi, buffer(8 + i * 8, 4))
        paging_record_subtree:add_le(paging_record_fields.cn_domain, buffer(12 + i * 8, 4))
    end
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(5000, rrc_paging_proto)
