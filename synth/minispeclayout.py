import re, sys

# $lic$
# Copyright (C) 2019-2020 by Daniel Sanchez
#
# This file is part of the Minispec compiler and toolset.
#
# Minispec is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, version 2.
#
# Minispec is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <http://www.gnu.org/licenses/>.

# Minispec type layout analysis
# To use this as a module, create a MinispecLayout object, which takes in a bsv
# source file as input and has a translate() method that translates raw signal
# bits into base types. 
#
# IMPORTANT: DO NOT USE THIS MODULE WITH ARBITRARY BSV CODE!
# This module analyzes the BSV output from msc because that's already
# elaborated, but it relies on Minispec interfaces and conventions!. In
# particular:
# - All parametrics must be elaborated, and types must be in msc format
# - All Integer parameters must be unsized decimals (msc enforces this)
# - Modules must follow msc conventions for inputs and methods
#
# This code fails silently if it can't translate a type, to allow interop with
# BSV. Finally, this code is sensitive to bsc 2016.07 layout conventions on
# structs, vectors, and Maybe types. 

def _canonicalizeBsv(input):
    # Remove comments
    res = re.sub('\/\*(.*?)\*\/', '', input, flags = re.MULTILINE | re.DOTALL)
    res = re.sub('\/\/(.*?)\n', '\n', res)
    # Make all whitespaces a single space
    res = re.sub('(\n|\r|\t)', ' ', res)
    res = re.sub(' +', ' ', res)
    # Remove spaces around relevant symbols
    res = re.sub(' ?(,|\.|{|}|#|\(|\)|\?|\:|\=|;|\') ?', '\\1', res)
    return res

# Parametric types have extraneous \ throughout. Nix them.
def _formatType(t):
    return re.sub(r'\\', '', t)

# Returns all the start/end locs of the first level of parens in the input
def _parseParenLocs(s):
    curLevel = 0
    res = []
    curStart = -1
    for i in range(len(s)):
        c = s[i]
        if c == "(":
            if curLevel == 0:
                curStart = i + 1
            curLevel += 1
        elif c == ")":
            curLevel -= 1
            if curLevel == 0:
                res.append((curStart, i))
    return res

# Returns contents of first-level parens in the input (parens not included)
def _parseParens(s):
    return [s[start:end] for (start, end) in _parseParenLocs(s)]

# Given a string that starts with a type, returns (type, restOfString)
# In the canonicalized BSV code we use, a type can be non-parametric, in which
# case it's trailed by a single space (e.g., "Bool foo"), or parametric, in
# which case there's no space (e.g., "Bit#(1)foo"). Handle both cases.
def _parseType(s):
    s = s.strip()
    if " " in s:
        (t, _, rest) = s.partition(" ")
        if "#" not in t:
            return (_formatType(t), rest)
    # This has a parametric type, delimited by the first set of parens
    locs = _parseParenLocs(s)
    splitPoint = locs[0][1] + 1
    return (_formatType(s[:splitPoint]), s[splitPoint:])

class MinispecLayout:
    def __init__(self, bsvCode, topLevelModule):
        bsv = _canonicalizeBsv(bsvCode)

        # 1. Get all registers in the design, and their flattened names
        submodDict = {}
        modules = re.findall("module (.+?);(.+?)endmodule", bsv)
        for (modProto, modBody) in modules:
            # Skip top-level wrapper module
            mkName = _formatType(modProto[:_parseParenLocs(modProto)[-1][0]-1])
            if mkName == "mkTopLevel___":
                continue
            modName = _formatType(_parseParens(modProto)[-1])
            # msc emits submodules, then rules, then methods,
            # so split at first rule to avoid matching actions
            submodsBody = modBody.split("rule ")[0]
            submods = re.findall("(.+?)<-(.+?);", submodsBody)
            # remove leading varAssign statements
            submods = [s.split(";")[-1] for (s, _) in submods]
            submodDict[modName] = [_parseType(submodHdr) for submodHdr in submods
                    if not submodHdr.strip().startswith("Wire#")]

        def getRegs(modName):
            # Flatten vectors of submodules
            m = re.match("Vector#\((\d+),(\S+)\)", modName)
            if m != None:
                elems = int(m.group(1))
                elemType = m.group(2)
                
                r = re.match("(Reg|RegU)#\((\S+)\)", elemType)
                if r != None:
                    return [(str(i), r.group(2)) for i in range(elems)]

                res = []
                elemRegs = getRegs(elemType)
                for i in range(elems):
                    res += [("_".join([str(i), r]), t) for (r, t) in elemRegs]
                return res

            # NOTE: This omits BSV submodules silently
            if not modName in submodDict:
                return []

            res = []
            for (t, s) in submodDict[modName]:
                if t.startswith("Reg#") or t.startswith("RegU#"):
                    res.append((s, _parseParens(t)[0]))
                else:
                    submodRegs = getRegs(t)
                    res += [("_".join([s, r]), rt) for (r, rt) in submodRegs]
            return res

        # Find interface for top-level module. This handles all corner cases
        # (functions, mkTopLevelModule___, etc.)
        topLevelModule = _formatType(topLevelModule)
        topLevelIfc = None
        isFunction = False
        for (modProto, modBody) in modules:
            mkName = _formatType(modProto[:_parseParenLocs(modProto)[-1][0]-1])
            if mkName == topLevelModule:
                topLevelIfc = _formatType(_parseParens(modProto)[-1])
                realMkName = mkName
                if mkName == "mkTopLevel___":
                    # This is a wrapper, the real mkName is:
                    realMkName = _formatType(modBody.split("<-")[1].split(";")[0].strip())
                isFunction = realMkName[2].islower()
        if topLevelIfc is None:
            print("ERROR: Top-level module", topLevelModule, "not found in generated BSV!?")
            sys.exit(-1)

        regs = dict(getRegs(topLevelIfc))


        # 2. Parse top-level interface to infer input and output wire types
        # (inputs: inputs + method args; outputs: method result)
        inputDict = {}
        outputDict = {}
        ifcs = re.findall("interface (.+?);(.*?)endinterface", bsv)
        for (ifcName, ifcBody) in ifcs:
            inputs = {}
            outputs = {}
            ifcName = _formatType(ifcName.strip())
            methods = re.findall("method (.+?);", ifcBody)
            for methodProto in methods:
                (methodType, rest) = _parseType(methodProto)
                methodName = rest.split("(")[0].strip()
                if methodType == "Action":
                    arg = _parseParens(rest)[0] # always 1 arg
                    (argType, argName) = _parseType(arg)
                    inputs[methodName + "_" + argName] = argType
                    inputs[methodName + "_enable"] = "Bool"
                else:
                    outputs[methodName] = methodType
                    if "(" in rest:
                        args = _parseParens(rest)[0]
                        # Parse args one by one, serially. We can't just split
                        # by commas because args with parametric types may have
                        # commas too. Instead, lop off 1st arg's type, then
                        # if there's a comma in the rest, that is the separator
                        # between the arg's name and the next arg
                        while len(args) > 0:
                            (argType, argsRest) = _parseType(args)
                            (argName, _, args) = argsRest.partition(",")
                            inputs[methodName + "_" + argName] = argType
            
            # Post-process function I/Os to follow synth formatting conventions
            if isFunction:
                inputs = dict([(k[3:], v) for (k, v) in inputs.items()])
                outputs = dict([("out", v) for (k, v) in outputs.items()])
            
            inputDict[ifcName] = inputs
            outputDict[ifcName] = outputs

        # Top-level IOs:
        inputs = inputDict[topLevelIfc].copy()
        outputs = outputDict[topLevelIfc].copy()

        # Seek out any BVI modules to add them to the top-level I/Os
        bviModules = dict([(_formatType(_parseParens(modProto)[-1]), _formatType(modProto[:_parseParenLocs(modProto)[-1][0]-1]))
                for (modProto, modBody) in re.findall('import "BVI" module (.+?);(.+?)endmodule', bsv)])
        def getBviSubmods(modName):
            res = []
            if modName not in submodDict:
                return res
            for (t, s) in submodDict[modName]:
                if t in bviModules:
                    res.append((s, t))
                else:
                    res += [("_".join([s, r]), rt) for (r, rt) in getBviSubmods(t)]
            return res

        for (submodName, bviType) in getBviSubmods(topLevelIfc):
            for (name, type) in inputDict[bviType].items():
                outputs[submodName + "_" + name] = type
            for (name, type) in outputDict[bviType].items():
                inputs[submodName + "_" + name] = type


        # 3. Capture all type definitions
        typeDict = {}
        structs = re.findall("typedef struct{(.*?)}(.+?);", bsv)
        for (membersStr, structProto) in structs:
            structType = _parseType(structProto)[0]
            members = [_parseType(mStr)[::-1] for mStr in re.findall("(.*?);", membersStr)]
            # BSV Layout: Earlier struct members take higher bits (~big-endian)
            # We repr compound types lower-fields-first, so we reverse members
            members.reverse()
            typeDict[structType] = members

        enums = re.findall("typedef enum{(.*?)}(.+?);", bsv)
        for (labelsStr, enumProto) in enums:
            enumType = _parseType(enumProto)[0]
            # Follow BSV's label assignment rules to find the max label id
            curId = -1
            maxId = 0
            for lStr in labelsStr.split(","):
                if "=" in lStr:
                    (l, _, id) = lStr.rpartition("=")
                    # Format as unsized (FIXME: Test thoroughly)
                    id = re.sub("(\d+)\'", "0", id)
                    id = re.sub("h", "x", id)
                    curId = int(id, 0) # infer base
                else:
                    curId = curId + 1
                maxId = max(maxId, curId)
            typeDict[enumType] = maxId.bit_length()

        synonyms = [s for s in re.findall("typedef (.+?);", bsv)
                if not s.startswith("struct")   # filter out structs
                   and not s.startswith("enum") # filter out enums
                   and "#(type" not in s]       # filter out MS prelude types
        for sStr in synonyms:
            (oldType, rest) = _parseType(sStr)
            newType = _formatType(rest.strip())
            typeDict[newType] = oldType

        def getUnresolvedTypes():
            res = []
            for v in typeDict.values():
                if isinstance(v, int):
                    continue
                elif isinstance(v, str):
                    if v not in typeDict:
                        res.append(v)
                else:
                    res += [t for (m, t) in v if t not in typeDict]
            return res

        # Refine unresolved types
        u = getUnresolvedTypes() + list(regs.values()) + list(inputs.values()) + list(outputs.values())
        while len(u) != 0:
            for t in u:
                if t in typeDict:
                    continue
                if t == "Bool":
                    typeDict[t] = 1
                    continue
                m = re.match("(Bit|Int|UInt)#\((\d+)\)", t)
                if m != None:
                    typeDict[t] = int(m.group(2))
                    continue
                m = re.match("Maybe#\((\S+)\)", t)
                if m != None:
                    # BSV Layout: In a Maybe type, the valid bit is the highest-order one
                    typeDict[t] = [("value", m.group(1)), ("valid", "Bool")]
                    continue
                m = re.match("Vector#\((\d+),(\S+)\)", t)
                if m != None:
                    # BSV Layout: Given n-bit elements, Vector element i takes bits [i*(n+1)-1:i*n]
                    typeDict[t] = [("_%d" % i, m.group(2)) for i in range(int(m.group(1)))]
                    continue
                # Uncomment to fail non-silently
                #print "ERROR: Can't understand type", t
                #sys.exit(-1)
                typeDict[t] = -1 # Undefined
            # Due to parametrics, this may go on for several rounds
            u = getUnresolvedTypes()

        # Finally, flatten all types
        def getLayout(t):
            v = typeDict[t]
            if isinstance(v, int):
                return v
            elif isinstance(v, str):
                return getLayout(v)
            else:
                layout = []
                for (m, mt) in v:
                    ml = getLayout(mt)
                    if isinstance(ml, int):
                        if ml == -1:
                            return -1 # Bail out, undefined member
                        layout.append((m, ml))
                    else:
                        layout += [(m + "." + sm, sl) for (sm, sl) in ml]
            return layout

        self.hasTopLevelWrapper = topLevelModule == "mkTopLevel___"
        self.typeLayout = dict([(t, getLayout(t)) for t in typeDict])
        self.regs = regs
        self.inputs = inputs
        self.outputs = outputs
        self.bviMkNames = set(bviModules.values())

    def translate(self, wire):
        # With a top-level wrapper module, we need demangling (in all cases)
        if self.hasTopLevelWrapper and wire.startswith("res_"):
            wire = wire[4:]

        if "[" not in wire:
            return wire
        wp = wire.split("[")
        wireName = wp[0].strip()
        (idxStr, _, wireSuffix) = wp[-1].partition("]")
        if ":" in idxStr:
            return wire
        idx = int(idxStr)
        wireType = None
        for tc in [self.inputs, self.outputs, self.regs]:
            if wireName in tc:
                wireType = tc[wireName]
        if wireType == None:
            return wire
        if wireType not in self.typeLayout:
            return wire
        layout = self.typeLayout[wireType]
        if isinstance(layout, int):
            # NOTE: Includes both basic types and undefined ones (-1)
            return wire
        offset = idx
        for (member, size) in layout:
            if size <= offset:
                offset -= size
            else:
                name = wireName + "." + member
                if size == 1:
                    return name + wireSuffix
                else:
                    return name + ("[%d]" % offset) + wireSuffix
        # Somehow we ran over... throw?
        return wire

    def getWidth(self, type):
        if type not in self.typeLayout:
            return -1
        layout = self.typeLayout[type]
        if isinstance(layout, int):
            return layout
        else:
            return sum([size for (member, size) in layout])

    def isBvi(self, mkName):
        return mkName in self.bviMkNames
