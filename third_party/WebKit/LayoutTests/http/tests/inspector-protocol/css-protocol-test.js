function initialize_cssTest()
{

InspectorTest.dumpStyleSheetText = function(styleSheetId, callback)
{
    InspectorTest.sendCommandOrDie("CSS.getStyleSheetText", { styleSheetId: styleSheetId }, onStyleSheetText);
    function onStyleSheetText(result)
    {
        InspectorTest.log("==== Style sheet text ====");
        InspectorTest.log(result.text);
        callback();
    }
}

function updateStyleSheetRange(command, styleSheetId, expectError, options, callback)
{
    options.styleSheetId = styleSheetId;
    if (expectError)
        InspectorTest.sendCommand(command, options, onResponse);
    else
        InspectorTest.sendCommandOrDie(command, options, onSuccess);

    function onSuccess()
    {
        InspectorTest.dumpStyleSheetText(styleSheetId, callback);
    }

    function onResponse(message)
    {
        if (!message.error) {
            InspectorTest.log("ERROR: protocol method call did not return expected error. Instead, the following message was received: " + JSON.stringify(message));
            InspectorTest.completeTest();
            return;
        }
        InspectorTest.log("Expected protocol error: " + message.error.message);
        callback();
    }
}

InspectorTest.setPropertyText = updateStyleSheetRange.bind(null, "CSS.setPropertyText");
InspectorTest.setRuleSelector = updateStyleSheetRange.bind(null, "CSS.setRuleSelector");
InspectorTest.setStyleText = updateStyleSheetRange.bind(null, "CSS.setStyleText");
InspectorTest.setMediaText = updateStyleSheetRange.bind(null, "CSS.setMediaText");
InspectorTest.addRule = updateStyleSheetRange.bind(null, "CSS.addRule");

InspectorTest.requestMainFrameId = function(callback)
{
    InspectorTest.sendCommandOrDie("Page.enable", {}, pageEnabled);

    function pageEnabled()
    {
        InspectorTest.sendCommandOrDie("Page.getResourceTree", {}, resourceTreeLoaded);
    }

    function resourceTreeLoaded(payload)
    {
        callback(payload.frameTree.frame.id);
    }
};

function indentLog(indent, string)
{
    var indentString = Array(indent+1).join(" ");
    InspectorTest.log(indentString + string);
}

InspectorTest.dumpRuleMatch = function(ruleMatch)
{

    var rule = ruleMatch.rule;
    var matchingSelectors = ruleMatch.matchingSelectors;
    var media = rule.media || [];
    var mediaLine = "";
    for (var i = 0; i < media.length; ++i)
        mediaLine += (i > 0 ? " " : "") + media[i].text;
    var baseIndent = 0;
    if (mediaLine.length) {
        indentLog(baseIndent, "@media " + mediaLine);
        baseIndent += 4;
    }
    var selectorLine = "";
    var selectors = rule.selectorList.selectors;
    for (var i = 0; i < selectors.length; ++i) {
        if (i > 0)
            selectorLine += ", ";
        var matching = matchingSelectors.indexOf(i) !== -1;
        if (matching)
            selectorLine += "*";
        selectorLine += selectors[i].value;
        if (matching)
            selectorLine += "*";
    }
    selectorLine += " {";
    selectorLine += "    " + rule.origin;
    if (!rule.style.styleSheetId)
        selectorLine += "    readonly";
    indentLog(baseIndent, selectorLine);
    InspectorTest.dumpStyle(rule.style, baseIndent);
    indentLog(baseIndent, "}");
};

InspectorTest.dumpStyle = function(style, baseIndent)
{
    if (!style)
        return;
    var cssProperties = style.cssProperties;
    for (var i = 0; i < cssProperties.length; ++i) {
        var cssProperty = cssProperties[i];
        var propertyLine = cssProperty.name + ": " + cssProperty.value + ";";
        indentLog(baseIndent + 4, propertyLine);
    }
}

InspectorTest.displayName = function(url)
{
    return url.substr(url.lastIndexOf("/") + 1);
};

InspectorTest.loadAndDumpMatchingRulesForNode = function(nodeId, callback, omitLog)
{
    InspectorTest.sendCommandOrDie("CSS.getMatchedStylesForNode", { "nodeId": nodeId }, matchingRulesLoaded);

    function matchingRulesLoaded(result)
    {
        if (!omitLog)
            InspectorTest.log("Dumping matched rules: ");
        dumpRuleMatches(result.matchedCSSRules);
        if (!omitLog)
            InspectorTest.log("Dumping inherited rules: ");
        for (var inheritedEntry of result.inherited) {
            InspectorTest.dumpStyle(inheritedEntry.inlineStyle);
            dumpRuleMatches(inheritedEntry.matchedCSSRules);
        }
        callback();
    }

    function dumpRuleMatches(ruleMatches)
    {
        for (var ruleMatch of ruleMatches) {
            var origin = ruleMatch.rule.origin;
            if (origin !== "inspector" && origin !== "regular")
                continue;
            InspectorTest.dumpRuleMatch(ruleMatch);
        }
    }
}

InspectorTest.loadAndDumpMatchingRules = function(documentNodeId, selector, callback, omitLog)
{
    InspectorTest.requestNodeId(documentNodeId, selector, nodeIdLoaded);

    function nodeIdLoaded(nodeId)
    {
        InspectorTest.loadAndDumpMatchingRulesForNode(nodeId, callback, omitLog);
    }
}

InspectorTest.loadAndDumpInlineAndMatchingRules = function(documentNodeId, selector, callback, omitLog)
{
    InspectorTest.requestNodeId(documentNodeId, selector, nodeIdLoaded);
    var nodeId;
    function nodeIdLoaded(id)
    {
        nodeId = id;
        InspectorTest.sendCommandOrDie("CSS.getInlineStylesForNode", { "nodeId": nodeId }, onInline);
    }

    function onInline(result)
    {
        if (!omitLog)
            InspectorTest.log("Dumping inline style: ");
        InspectorTest.log("{");
        InspectorTest.dumpStyle(result.inlineStyle, 0);
        InspectorTest.log("}");
        InspectorTest.loadAndDumpMatchingRulesForNode(nodeId, callback, omitLog)
    }
}
}
