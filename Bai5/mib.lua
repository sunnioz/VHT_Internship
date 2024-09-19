--[[
struct MIB
{
    char message_id;        /*Dinh danh cua ban tin*/
    short sfn_value;        /*Gia tri SFN*/
};
]]

-- Create a new protocol named MIB
local mib_proto = Proto("MIB", "MIB Message")

-- Define the fields for the MIB protocol
local fields = mib_proto.fields
fields.message_id = ProtoField.int8("mib.message_id", "Message ID", base.DEC)
fields.sfn_value = ProtoField.int16("mib.sfn_value", "SFN Value", base.DEC)

-- Dissector function
function mib_proto.dissector(buffer, pinfo, tree)
    -- Set the protocol column in the UI
    pinfo.cols.protocol = "MIB"

    -- Create the protocol tree
    local subtree = tree:add(mib_proto, buffer(), "MIB Message")

    -- Check if the buffer is large enough to contain the entire message
    if buffer:len() < 3 then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR, "MIB message length is too short")
        return
    end

    -- Add fields to the tree
    subtree:add_le(fields.message_id, buffer(0, 1))
    subtree:add_le(fields.sfn_value, buffer(2, 2))
end

-- Register the dissector to UDP port 5000
local udp_port = DissectorTable.get("udp.port")
udp_port:add(5000, mib_proto)
