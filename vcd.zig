const std = @import("std");
const mem = std.mem;
const fmt = std.fmt;
const heap = std.heap;
const ArrayList = std.array_list.AlignedManaged;

const SFL = 127;
const MAX_SCOPE = 32;
const MAX_CHANNEL = 400;
const ITV_TIME = 10;
const VALUES = "0123456789zZxXbU-";

const Token = [SFL + 1]u8;

const PrintOpt = struct {
    low: []const u8,
    raise: []const u8,
    high: []const u8,
    drown: []const u8,
    start: []const u8,
    end: []const u8,
    skip: u32,
};

const Sample = struct {
    state: u8,
    val: u32,
};

const Channel = struct {
    size: u32,
    scope: u32,
    name: Token,
    samples: ArrayList(Sample, null),

    fn init(allocator: mem.Allocator) Channel {
        return .{
            .size = 0,
            .scope = 0,
            .name = mem.zeroes(Token),
            .samples = ArrayList(Sample, null).init(allocator),
        };
    }

    fn deinit(self: *Channel) void {
        self.samples.deinit();
    }
};

const ParseCtx = struct {
    allocator: mem.Allocator,
    ch: [MAX_CHANNEL]Channel,
    scopes: [MAX_SCOPE]Token,
    total: u32,
    scope_count: u32,
    chan_str: bool,
    scale: f32,
    date: Token,
    version: Token,
    unit: Token,
    scope_cur: u32,
    scope_lim: u32,
    ch_lim: u32,
    sz_lim: u32,

    fn init(allocator: mem.Allocator) ParseCtx {
        var ctx = ParseCtx{
            .allocator = allocator,
            .ch = undefined,
            .scopes = undefined,
            .total = 0,
            .scope_count = 0,
            .chan_str = false,
            .scale = 0.0,
            .date = mem.zeroes(Token),
            .version = mem.zeroes(Token),
            .unit = mem.zeroes(Token),
            .scope_cur = 0,
            .scope_lim = 0,
            .ch_lim = 0,
            .sz_lim = 0,
        };
        // Initialize all channels with size=0 and empty sample lists
        for (&ctx.ch) |*c| {
            c.size = 0;
            c.scope = 0;
            c.name = mem.zeroes(Token);
            c.samples = ArrayList(Sample, null).init(allocator);
        }
        for (&ctx.scopes) |*s| {
            s.* = mem.zeroes(Token);
        }
        return ctx;
    }

    fn deinit(self: *ParseCtx) void {
        for (&self.ch) |*c| {
            c.deinit();
        }
    }
};

fn chanId(str_id: []const u8, isStr: bool) !usize {
    var id: usize = 0;
    if (isStr) {
        id = try fmt.parseInt(usize, str_id[1..], 10);
    } else {
        var i = str_id.len;
        while (i > 0) {
            i -= 1;
            id = (id * 94) + @as(usize, str_id[i] - '!');
        }
    }
    if (id >= MAX_CHANNEL) {
        return error.MaxChannelReached;
    }
    return id;
}

fn unilen(s: []const u8) usize {
    var j: usize = 0;
    for (s) |byte| {
        if ((byte & 0xc0) != 0x80) j += 1;
    }
    return j;
}

fn padString(writer: anytype, s: []const u8, width: usize) !void {
    try writer.writeAll(s);
    var i: usize = s.len;
    while (i < width) : (i += 1) {
        try writer.writeByte(' ');
    }
}

fn padFloat(writer: anytype, val: f64, width: usize) !void {
    var buf: [256]u8 = undefined;
    const s = try fmt.bufPrint(&buf, "{d}", .{val});
    try writer.writeAll(s);
    var i: usize = s.len;
    while (i < width) : (i += 1) {
        try writer.writeByte(' ');
    }
}

fn padChar(writer: anytype, c: u8, width: usize) !void {
    try writer.writeByte(c);
    var i: usize = 1;
    while (i < width) : (i += 1) {
        try writer.writeByte(' ');
    }
}

fn padHex(writer: anytype, val: u32, width: usize) !void {
    var buf: [256]u8 = undefined;
    const s = try fmt.bufPrint(&buf, "{X}", .{val});
    try writer.writeAll(s);
    var i: usize = s.len;
    while (i < width) : (i += 1) {
        try writer.writeByte(' ');
    }
}

fn parseVcd(p: *ParseCtx, content: []const u8) !void {
    var pos: usize = 0;
    
    while (pos < content.len) {
        // Skip whitespace
        while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
        if (pos >= content.len) break;
        
        const c = content[pos];
        
        if (c == '$') {
            // Parse instruction
            pos += 1;
            const token_start = pos;
            while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
            const token = content[token_start..pos];
            
            if (mem.eql(u8, token, "var")) {
                // Skip whitespace
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                // Read type
                const type_start = pos;
                while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                _ = content[type_start..pos]; // type, unused
                
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                // Read size
                const size_start = pos;
                while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                const size = try fmt.parseInt(u32, content[size_start..pos], 10);
                
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                // Read id
                const id_start = pos;
                while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                const id_str = content[id_start..pos];
                
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                // Read name
                const name_start = pos;
                while (pos < content.len and content[pos] != '$' and content[pos] != '\n') : (pos += 1) {}
                const name_str = mem.trim(u8, content[name_start..pos], " \t\r\n");
                
                const id = try chanId(id_str, p.chan_str);
                p.ch[id].size = size;
                p.ch[id].scope = p.scope_cur;
                @memset(&p.ch[id].name, 0);
                const name_len = @min(name_str.len, SFL);
                @memcpy(p.ch[id].name[0..name_len], name_str[0..name_len]);
                
                p.ch_lim = @max(p.ch_lim, @as(u32, @intCast(name_len)));
                p.sz_lim = @max(p.sz_lim, size);
            } else if (mem.eql(u8, token, "scope")) {
                p.scope_count += 1;
                if (p.scope_count >= MAX_SCOPE) {
                    return error.MaxScopeReached;
                }
                p.scope_cur = p.scope_count;
                
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                // Skip scope type
                while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                // Read scope name
                const scope_start = pos;
                while (pos < content.len and !std.ascii.isWhitespace(content[pos]) and content[pos] != '$') : (pos += 1) {}
                const scope_name = content[scope_start..pos];
                
                @memset(&p.scopes[p.scope_cur], 0);
                const len = @min(scope_name.len, SFL);
                @memcpy(p.scopes[p.scope_cur][0..len], scope_name[0..len]);
                p.scope_lim = @max(p.scope_lim, @as(u32, @intCast(len)));
            } else if (mem.eql(u8, token, "date")) {
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                const date_start = pos;
                while (pos < content.len and content[pos] != '$') : (pos += 1) {}
                const date_str = mem.trim(u8, content[date_start..pos], " \t\r\n");
                @memset(&p.date, 0);
                const len = @min(date_str.len, SFL);
                @memcpy(p.date[0..len], date_str[0..len]);
            } else if (mem.eql(u8, token, "version")) {
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                const ver_start = pos;
                while (pos < content.len and content[pos] != '$') : (pos += 1) {}
                const ver_str = mem.trim(u8, content[ver_start..pos], " \t\r\n");
                @memset(&p.version, 0);
                const len = @min(ver_str.len, SFL);
                @memcpy(p.version[0..len], ver_str[0..len]);
                p.chan_str = mem.indexOf(u8, &p.version, "ROHD") != null;
            } else if (mem.eql(u8, token, "timescale")) {
                while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
                const timescale_start = pos;
                while (pos < content.len and content[pos] != '$' and content[pos] != '\n') : (pos += 1) {}
                const timescale_str = mem.trim(u8, content[timescale_start..pos], " \t\r\n");
                
                // Parse timescale which can be "1 fs" or "1ps"
                var scale_end: usize = 0;
                while (scale_end < timescale_str.len and (std.ascii.isDigit(timescale_str[scale_end]) or timescale_str[scale_end] == '.')) : (scale_end += 1) {}
                p.scale = try fmt.parseFloat(f32, timescale_str[0..scale_end]);
                
                // Extract unit (rest of the string after the number)
                const unit_str = mem.trim(u8, timescale_str[scale_end..], " \t");
                @memset(&p.unit, 0);
                const len = @min(unit_str.len, SFL);
                if (len > 0) @memcpy(p.unit[0..len], unit_str[0..len]);
            } else if (mem.eql(u8, token, "upscope")) {
                p.scope_cur = 0;
            }
            
            // Skip to $end
            while (pos < content.len and content[pos] != '$') : (pos += 1) {}
            // Skip "$end"
            if (pos < content.len and content[pos] == '$') {
                pos += 1;
                while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
            }
        } else if (c == '#') {
            // Timestamp
            pos += 1;
            const ts_start = pos;
            while (pos < content.len and std.ascii.isDigit(content[pos])) : (pos += 1) {}
            _ = content[ts_start..pos]; // timestamp value, unused
            
            // Copy previous sample on every channel
            for (&p.ch) |*ch| {
                if (ch.size == 0) continue;
                
                // Ensure channel has enough samples (fill missing ones with zeros)
                while (ch.samples.items.len < p.total) {
                    try ch.samples.append(.{ .state = 0, .val = 0 });
                }
                
                // Now append the new sample
                if (p.total > 0 and ch.samples.items.len > 0) {
                    const prev = ch.samples.items[p.total - 1];
                    try ch.samples.append(prev);
                } else {
                    try ch.samples.append(.{ .state = 0, .val = 0 });
                }
            }
            p.total += 1;
        } else if (mem.indexOfScalar(u8, VALUES, c) != null) {
            // Sample
            var s = Sample{ .state = 0, .val = 0 };
            
            if (c == 'b') {
                pos += 1;
                while (pos < content.len and !std.ascii.isWhitespace(content[pos])) {
                    const byte = content[pos];
                    if (byte == '0' or byte == '1') {
                        s.val = s.val * 2 + (byte - '0');
                    } else if (mem.indexOfScalar(u8, VALUES, byte) != null) {
                        s.state = byte;
                    }
                    pos += 1;
                }
            } else {
                s.state = if (std.ascii.isAlphabetic(c)) c else 0;
                s.val = if (std.ascii.isDigit(c)) c - '0' else 0;
                pos += 1;
            }
            
            // Skip whitespace
            while (pos < content.len and std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
            
            // Read channel ID
            const id_start = pos;
            while (pos < content.len and !std.ascii.isWhitespace(content[pos])) : (pos += 1) {}
            const id_str = content[id_start..pos];
            
            if (p.total > 0 and id_str.len > 0) {
                const id = try chanId(id_str, p.chan_str);
                if (p.ch[id].samples.items.len > 0) {
                    p.ch[id].samples.items[p.total - 1] = s;
                }
            }
        } else {
            pos += 1;
        }
    }
}

fn printYml(p: *ParseCtx, opt: *const PrintOpt, stdout: std.fs.File) !void {
    if (unilen(opt.high) != 1 or unilen(opt.low) != 1) {
        return error.InvalidWaveform;
    }
    if (unilen(opt.drown) > 1 or unilen(opt.raise) > 1) {
        return error.InvalidWaveform;
    }
    
    const zoom: i32 = @intCast((p.sz_lim + 7) >> 2);
    const trans = opt.drown.len > 0 and opt.raise.len > 0;
    
    var output = ArrayList(u8, null).init(p.allocator);
    defer output.deinit();
    const writer = output.writer();
    
    try writer.print("global:\n", .{});
    try writer.print("  zoom: {d}\n", .{zoom});
    try writer.print("  date: {s}\n", .{mem.sliceTo(&p.date, 0)});
    try writer.print("  total: {d}\n", .{p.total});
    try writer.print("  skip: {d}\n", .{opt.skip});
    try writer.print("  time:\n", .{});
    try writer.print("    scale: {d:.2}\n", .{p.scale});
    const unit_str = mem.sliceTo(&p.unit, 0);
    try writer.print("    unit: {s}\n", .{if (unit_str.len > 0) unit_str else "?"});
    try writer.writeAll("    ");
    try padString(writer, "line", p.ch_lim);
    try writer.writeAll(": ");
    try writer.writeAll(opt.start);
    
    var smpl: f64 = @floatFromInt(opt.skip);
    const zoom_usize: usize = @intCast(zoom);
    while (smpl < @as(f64, @floatFromInt(p.total))) : (smpl += ITV_TIME) {
        try padFloat(writer, smpl * p.scale, ITV_TIME * zoom_usize - 1);
    }
    try writer.writeAll(opt.end);
    try writer.writeAll("\nchannels:\n");
    
    var prev_scope: ?u32 = null;
    for (&p.ch) |*ch| {
        if (ch.size == 0) continue;
        
        if (prev_scope == null or prev_scope.? != ch.scope) {
            const scope_name = mem.sliceTo(&p.scopes[ch.scope], 0);
            const name = if (scope_name.len > 0) scope_name else "default";
            try writer.print("  {s}:\n", .{name});
            prev_scope = ch.scope;
        }
        
        const ch_name = mem.sliceTo(&ch.name, 0);
        try writer.writeAll("    ");
        try padString(writer, ch_name, p.ch_lim);
        try writer.writeAll(": ");
        try writer.writeAll(opt.start);
        
        var sample_idx = opt.skip;
        while (sample_idx < p.total) : (sample_idx += 1) {
            if (sample_idx >= ch.samples.items.len) break;
            const s = ch.samples.items[sample_idx];
            const prev = if (sample_idx > 0 and sample_idx - 1 < ch.samples.items.len)
                ch.samples.items[sample_idx - 1]
            else
                s;
            
            if (s.state != 0) {
                try padChar(writer, s.state, zoom_usize);
            } else if (ch.size == 1) {
                var w: usize = 0;
                while (w < zoom_usize) : (w += 1) {
                    if (w == 0 and trans and s.state == prev.state and s.val != prev.val) {
                        try writer.writeAll(if (prev.val != 0) opt.drown else opt.raise);
                    } else {
                        try writer.writeAll(if (s.val != 0) opt.high else opt.low);
                    }
                }
            } else {
                try padHex(writer, s.val, zoom_usize);
            }
        }
        try writer.writeAll(opt.end);
        try writer.writeByte('\n');
    }
    
    try stdout.writeAll(output.items);
}

pub fn main() !void {
    var gpa = heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();
    
    const stdin = std.fs.File{ .handle = std.posix.STDIN_FILENO };
    const stdout = std.fs.File{ .handle = std.posix.STDOUT_FILENO };
    
    // Read all input
    const content = try stdin.readToEndAlloc(allocator, 100 * 1024 * 1024);
    defer allocator.free(content);
    
    var opt = PrintOpt{
        .low = std.posix.getenv("LOW") orelse "▁",
        .raise = std.posix.getenv("RAISE") orelse "╱",
        .high = std.posix.getenv("HIGH") orelse "▔",
        .drown = std.posix.getenv("DROWN") orelse "╲",
        .start = std.posix.getenv("STX") orelse "\"",
        .end = std.posix.getenv("ETX") orelse "\"",
        .skip = blk: {
            if (std.posix.getenv("SKIP")) |skip_str| {
                break :blk try fmt.parseInt(u32, skip_str, 10);
            }
            break :blk 0;
        },
    };
    
    var ctx = ParseCtx.init(allocator);
    defer ctx.deinit();
    
    try parseVcd(&ctx, content);
    try printYml(&ctx, &opt, stdout);
}
