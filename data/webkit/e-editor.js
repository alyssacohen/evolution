/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2019 Red Hat (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

'use strict';

/* semi-convention: private functions start with lower-case letter,
   public functions start with upper-case letter. */

var EvoEditor = {
	CURRENT_ELEMENT_ATTR : "x-evo-dialog-current-element",
	BLOCKQUOTE_STYLE : "margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex",

	E_CONTENT_EDITOR_ALIGNMENT_NONE		: -1,
	E_CONTENT_EDITOR_ALIGNMENT_LEFT		: 0,
	E_CONTENT_EDITOR_ALIGNMENT_CENTER	: 1,
	E_CONTENT_EDITOR_ALIGNMENT_RIGHT	: 2,
	E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY	: 3,

	E_CONTENT_EDITOR_BLOCK_FORMAT_NONE	: 0,
	E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH	: 1,
	E_CONTENT_EDITOR_BLOCK_FORMAT_PRE	: 2,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS	: 3,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H1	: 4,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H2	: 5,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H3	: 6,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H4	: 7,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H5	: 8,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H6	: 9,
	E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST : 10,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST : 11,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN : 12,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA : 13,

	E_CONTENT_EDITOR_GET_INLINE_IMAGES	: 1 << 0,
	E_CONTENT_EDITOR_GET_RAW_BODY_HTML	: 1 << 1,
	E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN	: 1 << 2,
	E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED	: 1 << 3,
	E_CONTENT_EDITOR_GET_RAW_DRAFT		: 1 << 4,
	E_CONTENT_EDITOR_GET_TO_SEND_HTML	: 1 << 5,
	E_CONTENT_EDITOR_GET_TO_SEND_PLAIN	: 1 << 6,

	E_CONTENT_EDITOR_NODE_UNKNOWN		: 0,
	E_CONTENT_EDITOR_NODE_IS_ANCHOR		: 1 << 0,
	E_CONTENT_EDITOR_NODE_IS_H_RULE		: 1 << 1,
	E_CONTENT_EDITOR_NODE_IS_IMAGE		: 1 << 2,
	E_CONTENT_EDITOR_NODE_IS_TABLE		: 1 << 3,
	E_CONTENT_EDITOR_NODE_IS_TABLE_CELL	: 1 << 4,
	E_CONTENT_EDITOR_NODE_IS_TEXT		: 1 << 5,
	E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED	: 1 << 6,

	E_CONTENT_EDITOR_SCOPE_CELL		: 0,
	E_CONTENT_EDITOR_SCOPE_ROW		: 1,
	E_CONTENT_EDITOR_SCOPE_COLUMN		: 2,
	E_CONTENT_EDITOR_SCOPE_TABLE		: 3,

	/* Flags for ClaimAffectedContent() */
	CLAIM_CONTENT_FLAG_NONE : 0,
	CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE : 1 << 0,
	CLAIM_CONTENT_FLAG_SAVE_HTML : 1 << 1,

	TEXT_INDENT_SIZE : 3, // in characters
	NORMAL_PARAGRAPH_WIDTH : 71,
	MAGIC_LINKS : true,
	MAGIC_SMILEYS : false,
	UNICODE_SMILEYS : false,
	WRAP_QUOTED_TEXT_IN_REPLIES : true,
	START_BOTTOM : false,
	TOP_SIGNATURE : false,

	FORCE_NO : 0,
	FORCE_YES : 1,
	FORCE_MAYBE : 2,

	MODE_PLAIN_TEXT : 0,
	MODE_HTML : 1,

	mode : 1, // one of the MODE constants
	storedSelection : null,
	propertiesSelection : null, // dedicated to Properties dialogs
	contextMenuNode : null, // the last target node for context menu
	inheritThemeColors : false,
	checkInheritFontsOnChange : false,
	forceFormatStateUpdate : false,
	formattingState : {
		mode : -1,
		anchorElement : null, // to avoid often notifications when just moving within the same node
		bold : false,
		italic : false,
		underline : false,
		strikethrough : false,
		script : 0, // -1..subscript, 0..normal, +1..superscript
		blockFormat : -1,
		alignment : -1,
		fgColor : null,
		bgColor : null,
		fontSize : null,
		fontFamily : null,
		indentLevel : 0,
		bodyFgColor : null,
		bodyBgColor : null,
		bodyLinkColor : null,
		bodyVlinkColor : null,
		bodyFontFamily : null
	}
};

EvoEditor.maybeUpdateFormattingState = function(force)
{
	var anchorElem = null;

	if (!document.getSelection().isCollapsed) {
		var commonParent;

		commonParent = EvoEditor.GetCommonParent(document.getSelection().anchorNode, document.getSelection().focusNode, true);
		if (commonParent) {
			var child1, child2;

			child1 = EvoEditor.GetDirectChild(commonParent, document.getSelection().anchorNode);
			child2 = EvoEditor.GetDirectChild(commonParent, document.getSelection().focusNode);

			if (child1 && (!child2 || (child2 && EvoEditor.GetChildIndex(commonParent, child1) <= EvoEditor.GetChildIndex(commonParent, child2)))) {
				anchorElem = document.getSelection().focusNode;
			}
		}
	}

	if (!anchorElem)
		anchorElem = document.getSelection().anchorNode;
	if (!anchorElem)
		anchorElem = document.body ? document.body.firstElementChild : null;

	if (anchorElem && anchorElem.nodeType == anchorElem.TEXT_NODE)
		anchorElem = anchorElem.parentElement;

	if (force == EvoEditor.FORCE_NO && EvoEditor.formattingState.anchorElement === anchorElem && EvoEditor.mode == EvoEditor.formattingState.mode) {
		return;
	}

	force = force == EvoEditor.FORCE_YES;

	EvoEditor.formattingState.anchorElement = anchorElem;

	var changes = {}, nchanges = 0, value, tmp;

	value = EvoEditor.mode;
	if (value != EvoEditor.formattingState.mode) {
		EvoEditor.formattingState.mode = value;
		changes["mode"] = value;
		nchanges++;
	}

	value = document.body ? document.body.style.fontFamily : "";
	if (force || value != EvoEditor.formattingState.bodyFontFamily) {
		EvoEditor.formattingState.bodyFontFamily = value;
		changes["bodyFontFamily"] = value;
		nchanges++;
	}

	value = document.body ? document.body.text : "";
	if (force || value != EvoEditor.formattingState.bodyFgColor) {
		EvoEditor.formattingState.bodyFgColor = value;
		changes["bodyFgColor"] = value;
		nchanges++;
	}

	value = document.body.bgColor;
	if (force || value != EvoEditor.formattingState.bodyBgColor) {
		EvoEditor.formattingState.bodyBgColor = value;
		changes["bodyBgColor"] = value;
		nchanges++;
	}

	value = document.body.link;
	if (force || value != EvoEditor.formattingState.bodyLinkColor) {
		EvoEditor.formattingState.bodyLinkColor = value;
		changes["bodyLinkColor"] = value;
		nchanges++;
	}

	value = document.body.vLink;
	if (force || value != EvoEditor.formattingState.bodyVlinkColor) {
		EvoEditor.formattingState.bodyVlinkColor = value;
		changes["bodyVlinkColor"] = value;
		nchanges++;
	}

	var parent, obj = {
		script : 0,
		blockFormat : null,
		indentLevel : 0,
		fontFamily : null,
		fontSize : null,
		fgColor : null,
		bgColor : null,
		bold : null,
		italic : null,
		underline : null,
		strikethrough : null,
		textAlign : null
	};

	for (parent = anchorElem; parent && !(parent === document.body); parent = parent.parentElement) {
		if (obj.script == 0) {
			if (parent.tagName == "SUB")
				obj.script = -1;
			else if (parent.tagName == "SUP")
				obj.script = +1;
		}

		if (obj.blockFormat === null) {
			if (parent.tagName == "DIV")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
			else if (parent.tagName == "PRE")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_PRE;
			else if (parent.tagName == "ADDRESS")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS;
			else if (parent.tagName == "H1")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H1;
			else if (parent.tagName == "H2")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H2;
			else if (parent.tagName == "H3")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H3;
			else if (parent.tagName == "H4")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H4;
			else if (parent.tagName == "H5")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H5;
			else if (parent.tagName == "H6")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H6;
			else if (parent.tagName == "UL")
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST;
			else if (parent.tagName == "OL") {
				obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST;

				var typeAttr = parent.getAttribute("type");

				if (typeAttr && typeAttr.toUpperCase() == "I")
					obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN;
				else if (typeAttr && typeAttr.toUpperCase() == "A")
					obj.blockFormat = EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA;
			}
		}

		if ((obj.fontSize === null || obj.fontFamily === null || obj.fgColor === null) &&
		    parent.tagName == "FONT") {
			if (obj.fontSize === null && parent.hasAttribute("size")) {
				value = parent.getAttribute("size");
				value = value ? parseInt(value, 10) : 0;
				if (Number.isInteger(value) && value >= 1 && value <= 7) {
					obj.fontSize = value;
				}
			}

			if (obj.fontFamily === null && parent.hasAttribute("face"))
				obj.fontFamily = parent.getAttribute("face");

			if (obj.fgColor === null && parent.hasAttribute("color"))
				obj.fgColor = parent.getAttribute("color");
		}

		var dir = window.getComputedStyle(parent).direction;

		if (dir == "rtl") {
			tmp = parent.style.marginRight;
			if (tmp && tmp.endsWith("ch")) {
				tmp = parseInt(tmp.slice(0, -2));
			} else {
				tmp = "";
			}
		} else { // "ltr" or other
			tmp = parent.style.marginLeft;
			if (tmp && tmp.endsWith("ch")) {
				tmp = parseInt(tmp.slice(0, -2));
			} else {
				tmp = "";
			}
		}

		if (Number.isInteger(tmp) && tmp > 0) {
			obj.indentLevel += tmp / EvoEditor.TEXT_INDENT_SIZE;
		}

		if (parent.tagName == "UL" || parent.tagName == "OL")
			obj.indentLevel++;

		if (obj.bgColor === null && parent.style.backgroundColor)
			obj.bgColor = parent.style.backgroundColor;

		if (obj.bold === null && parent.tagName == "B")
			obj.bold = true;

		if (obj.italic === null && parent.tagName == "I")
			obj.italic = true;

		if (obj.underline === null && parent.tagName == "U")
			obj.underline = true;

		if (obj.strikethrough === null && (parent.tagName == "S" || parent.tagName == "STRIKE"))
			obj.strikethrough = true;

		if (obj.textAlign === null && parent.style.textAlign)
			obj.textAlign = parent.style.textAlign;
	}

	value = obj.script;
	if (force || value != EvoEditor.formattingState.script) {
		EvoEditor.formattingState.script = value;
		changes["script"] = value;
		nchanges++;
	}

	value = obj.blockFormat === null ? EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH : obj.blockFormat;
	if (force || value != EvoEditor.formattingState.blockFormat) {
		EvoEditor.formattingState.blockFormat = value;
		changes["blockFormat"] = value;
		nchanges++;
	}

	value = obj.fontSize === null ? 3 /* E_CONTENT_EDITOR_FONT_SIZE_NORMAL */ : obj.fontSize;
	if (force || value != EvoEditor.formattingState.fontSize) {
		EvoEditor.formattingState.fontSize = value;
		changes["fontSize"] = value;
		nchanges++;
	}

	value = obj.indentLevel;
	if (force || value != EvoEditor.formattingState.indentLevel) {
		EvoEditor.formattingState.indentLevel = value;
		changes["indentLevel"] = value;
		nchanges++;
	}

	value = obj.fgColor ? obj.fgColor : "";
	if (force || value != EvoEditor.formattingState.fgColor) {
		EvoEditor.formattingState.fgColor = value;
		changes["fgColor"] = value;
		nchanges++;
	}

	value = obj.bgColor ? obj.bgColor : "";
	if (force || value != EvoEditor.formattingState.bgColor) {
		EvoEditor.formattingState.bgColor = value;
		changes["bgColor"] = value;
		nchanges++;
	}

	value = obj.bold ? true : false;
	if (value != EvoEditor.formattingState.bold) {
		EvoEditor.formattingState.bold = value;
		changes["bold"] = value;
		nchanges++;
	}

	value = obj.italic ? true : false;
	if (force || value != EvoEditor.formattingState.italic) {
		EvoEditor.formattingState.italic = value;
		changes["italic"] = value;
		nchanges++;
	}

	value = obj.underline ? true : false;
	if (force || value != EvoEditor.formattingState.underline) {
		EvoEditor.formattingState.underline = value;
		changes["underline"] = value;
		nchanges++;
	}

	value = obj.strikethrough ? true : false;
	if (force || value != EvoEditor.formattingState.strikethrough) {
		EvoEditor.formattingState.strikethrough = value;
		changes["strikethrough"] = value;
		nchanges++;
	}

	value = obj.fontFamily ? obj.fontFamily : "";
	// dequote the font name, if needed
	if (value.length > 1 && value.charAt(0) == '\"' && value.charAt(value.length - 1) == '\"')
		value = value.substr(1, value.length - 2);
	if (force || value != EvoEditor.formattingState.fontFamily) {
		EvoEditor.formattingState.fontFamily = value;
		changes["fontFamily"] = (!document.body || window.getComputedStyle(document.body).fontFamily == value) ? "" : value;
		nchanges++;
	}

	tmp = (obj.textAlign ? obj.textAlign : "").toLowerCase();
	if (tmp == "left" || tmp == "start")
		value = EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	else if (tmp == "right")
		value = EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
	else if (tmp == "center")
		value = EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_CENTER;
	else if (tmp == "justify")
		value = EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY;
	else if ((anchorElem ? window.getComputedStyle(anchorElem).direction : "").toLowerCase() == "rtl")
		value = EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
	else
		value = EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	if (force || value != EvoEditor.formattingState.alignment) {
		EvoEditor.formattingState.alignment = value;
		changes["alignment"] = value;
		nchanges++;
	}

	if (force) {
		changes["forced"] = true;
		nchanges++;
	}

	if (nchanges > 0)
		window.webkit.messageHandlers.formattingChanged.postMessage(changes);
}

EvoEditor.IsBlockNode = function(node)
{
	if (!node || !node.tagName) {
		return false;
	}

	return node.tagName == "BLOCKQUOTE" ||
		node.tagName == "DIV" ||
		node.tagName == "P" ||
		node.tagName == "PRE" ||
		node.tagName == "ADDRESS" ||
		node.tagName == "H1" ||
		node.tagName == "H2" ||
		node.tagName == "H3" ||
		node.tagName == "H4" ||
		node.tagName == "H5" ||
		node.tagName == "H6" ||
		node.tagName == "TD" ||
		node.tagName == "TH" ||
		node.tagName == "UL" ||
		node.tagName == "OL";
}

EvoEditor.foreachChildRecur = function(topParent, parent, firstChildIndex, lastChildIndex, traversar)
{
	if (!parent) {
		return false;
	}

	if (firstChildIndex >= parent.children.length) {
		return true;
	}

	var ii, child, next;

	ii = lastChildIndex - firstChildIndex;
	child = parent.children.item(firstChildIndex);

	while (child && ii >= 0) {
		next = child.nextElementSibling;

		if (child.children.length > 0 &&
		    !traversar.flat &&
		    !EvoEditor.foreachChildRecur(topParent, child, 0, child.children.length - 1, traversar)) {
			return false;
		}

		if (!traversar.onlyBlockElements || EvoEditor.IsBlockNode(child)) {
			if (!traversar.exec(topParent, child)) {
				return false;
			}
		}

		child = next;
		ii--;
	}

	return true;
}

/*
   Traverses children of the 'parent', between the 'firstChildIndex' and
   the 'lastChildIndex', where both indexes are meant inclusive.

   The 'traversar' is an object, which should contain at least function:

      bool exec(parent, element);

   which does its work in the 'element' and returns true, when the traversar
   should continue. The 'parent' is the one with which the funcion had been
   called with. The 'traversar' can also contain properties:

      bool flat;
      bool onlyBlockElements;

   the 'flat', if set to true, traverses only direct children of the parent,
   otherwise it dives into the hierarchy;

   the 'onlyBlockElements', if set to true, calls exec() only on elements,
   which are block elements (as of EvoEditor.IsBlockNode()), otherwise it
   is called for each element on the way.
*/
EvoEditor.ForeachChild = function(parent, firstChildIndex, lastChildIndex, traversar)
{
	return EvoEditor.foreachChildRecur(parent, parent, firstChildIndex, lastChildIndex, traversar);
}

EvoEditor.GetParentBlockNode = function(node)
{
	while (node && !EvoEditor.IsBlockNode(node) && node.tagName != "BODY") {
		node = node.parentElement;
	}

	return node;
}

EvoEditor.GetCommonParent = function(firstNode, secondNode, longPath)
{
	if (!firstNode || !secondNode) {
		return null;
	}

	if (firstNode.nodeType == firstNode.TEXT_NODE) {
		firstNode = firstNode.parentElement;
	}

	if (secondNode.nodeType == secondNode.TEXT_NODE) {
		secondNode = secondNode.parentElement;
	}

	if (!firstNode || !secondNode) {
		return null;
	}

	if (firstNode === document.body || secondNode === document.body) {
		return document.body;
	}

	var commonParent, secondParent;

	for (commonParent = (longPath ? firstNode : firstNode.parentElement); commonParent; commonParent = commonParent.parentElement) {
		if (commonParent === document.body) {
			break;
		}

		for (secondParent = (longPath ? secondNode : secondNode.parentElement); secondParent; secondParent = secondParent.parentElement) {
			if (secondParent === document.body) {
				break;
			}

			if (secondParent === commonParent) {
				return commonParent;
			}
		}
	}

	return document.body;
}

EvoEditor.GetDirectChild = function(parent, child)
{
	if (!parent || !child || parent === child) {
		return null;
	}

	while (child && !(child.parentElement === parent)) {
		child = child.parentElement;
	}

	return child;
}

EvoEditor.GetChildIndex = function(parent, child)
{
	if (!parent || !child)
		return -1;

	var ii;

	for (ii = 0; ii < parent.children.length; ii++) {
		if (child === parent.children.item(ii))
			return ii;
	}

	return -1;
}

EvoEditor.ClaimAffectedContent = function(startNode, endNode, flags)
{
	var commonParent, startChild, endChild;
	var firstChildIndex = -1, html = "", ii;
	var withHtml = (flags & EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML) != 0;
	var currentElemsArray = null;

	if (!startNode) {
		startNode = document.getSelection().anchorNode;
		endNode = document.getSelection().focusNode;

		if (!startNode) {
			startNode = document.body;
		}
	}

	if (!endNode) {
		endNode = document.getSelection().focusNode;

		if (!endNode)
			endNode = startNode;
	}

	if ((flags & EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE) != 0) {
		while (startNode && !(startNode === document.body)) {
			if (EvoEditor.IsBlockNode(startNode)) {
				break;
			}

			startNode = startNode.parentElement;
		}
	}

	if (withHtml) {
		var node = startNode;

		// cannot store only part of the HTML in a TABLE, only whole, because restoring
		// for example only "<td>text</td>" into a floating element drops the <td/>
		if (!node.tagName)
			node = node.parentElement;

		if (node.tagName == "TH" || node.tagName == "TD" || node.tagName == "TR") {
			node = EvoEditor.getParentElement("TABLE", node, true);
			if (node)
				startNode = node;
		}

		currentElemsArray = EvoEditor.RemoveCurrentElementAttr();
	}

	commonParent = EvoEditor.GetCommonParent(startNode, endNode, false);
	startChild = EvoEditor.GetDirectChild(commonParent, startNode);
	endChild = EvoEditor.GetDirectChild(commonParent, endNode);

	for (ii = 0 ; ii < commonParent.children.length; ii++) {
		var child = commonParent.children.item(ii);

		if (firstChildIndex == -1) {
			/* The selection can be made both from the top to the bottom and
			   from the bottom to the top, thus cover both cases. */
			if (child === startChild) {
				firstChildIndex = ii;
			} else if (child === endChild) {
				endChild = startChild;
				startChild = child;
				firstChildIndex = ii;
			}
		}

		if (firstChildIndex != -1) {
			if (withHtml) {
				html += child.outerHTML;
			}

			if (child === endChild) {
				ii++;
				break;
			}
		}
	}

	var affected = {};

	affected.path = EvoSelection.GetChildPath(document.body, commonParent);
	affected.firstChildIndex = firstChildIndex;
	affected.restChildrenCount = commonParent.children.length - ii;

	if (withHtml) {
		if (firstChildIndex == -1)
			affected.html = commonParent.innerHTML;
		else
			affected.html = html;

		EvoEditor.RestoreCurrentElementAttr(currentElemsArray);
	}

	return affected;
}

/* Calls EvoEditor.ForeachChild() on a content described by 'affected',
   which is result of EvoEditor.ClaimAffectedContent(). */
EvoEditor.ForeachChildInAffectedContent = function(affected, traversar)
{
	if (!affected || !traversar) {
		throw "EvoEditor.ForeachChildInAffectedContent: No 'affected' or 'traversar'";
	}

	var parent, firstChildIndex, lastChildIndex;

	parent = EvoSelection.FindElementByPath(document.body, affected.path);
	if (!parent) {
		throw "EvoEditor.ForeachChildInAffectedContent: Cannot find parent";
	}

	firstChildIndex = affected.firstChildIndex;
	/* Cannot subtract one, when none left, because the child index is inclusive */
	lastChildIndex = parent.children.length - affected.restChildrenCount + (affected.restChildrenCount ? -1 : 0);

	return EvoEditor.ForeachChild(parent, firstChildIndex, lastChildIndex, traversar);
}

EvoEditor.EmitContentChanged = function()
{
	if (window.webkit.messageHandlers.contentChanged)
		window.webkit.messageHandlers.contentChanged.postMessage(null);
}

EvoEditor.StoreSelection = function()
{
	EvoEditor.storedSelection = EvoSelection.Store(document);
}

EvoEditor.RestoreSelection = function()
{
	if (EvoEditor.storedSelection) {
		EvoSelection.Restore(document, EvoEditor.storedSelection);
		EvoEditor.storedSelection = null;
	}
}

EvoEditor.removeEmptyStyleAttribute = function(element)
{
	if (element && !element.style.length)
		element.removeAttribute("style");
}

EvoEditor.applySetAlignment = function(record, isUndo)
{
	if (record.changes) {
		var ii, parent, child;

		parent = EvoSelection.FindElementByPath(document.body, record.path);
		if (!parent) {
			throw "EvoEditor.applySetAlignment: Cannot find parent at path " + record.path;
		}

		for (ii = 0; ii < record.changes.length; ii++) {
			var change = record.changes[isUndo ? (record.changes.length - ii - 1) : ii];

			child = EvoSelection.FindElementByPath(parent, change.path);
			if (!child) {
				throw "EvoEditor.applySetAlignment: Cannot find child";
			}

			if (isUndo) {
				child.style.textAlign = change.before;
			} else if ((record.applyValueAfter == "left" && child.style.direction != "rtl" && window.getComputedStyle(child).direction != "rtl") ||
				   (record.applyValueAfter == "right" && (child.style.direction == "rtl" || window.getComputedStyle(child).direction == "rtl"))) {
				child.style.textAlign = "";
			} else {
				child.style.textAlign = record.applyValueAfter;
			}

			EvoEditor.removeEmptyStyleAttribute(child);
		}
	}
}

EvoEditor.SetAlignment = function(alignment)
{
	var traversar = {
		record : null,
		toSet : null,
		anyChanged : false,

		flat : false,
		onlyBlockElements : true,

		exec : function(parent, element) {
			if (window.getComputedStyle(element, null).textAlign != traversar.toSet) {
				if (traversar.record) {
					if (!traversar.record.changes)
						traversar.record.changes = [];

					var change = {};

					change.path = EvoSelection.GetChildPath(parent, element);
					change.before = element.style.textAlign;

					traversar.record.changes[traversar.record.changes.length] = change;
				}

				traversar.anyChanged = true;

				if ((traversar.toSet == "left" && element.style.direction != "rtl" && window.getComputedStyle(element).direction != "rtl") ||
				    (traversar.toSet == "right" && (element.style.direction == "rtl" || window.getComputedStyle(element).direction == "rtl"))) {
					element.style.textAlign = "";
				} else {
					element.style.textAlign = traversar.toSet;
				}

				EvoEditor.removeEmptyStyleAttribute(element);
			}

			return true;
		}
	};

	var affected = EvoEditor.ClaimAffectedContent(null, null, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);

	switch (alignment) {
	case EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_NONE:
		traversar.toSet = "";
		break;
	case EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_LEFT:
		traversar.toSet = "left";
		break;
	case EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_CENTER:
		traversar.toSet = "center";
		break;
	case  EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_RIGHT:
		traversar.toSet = "right";
		break;
	case EvoEditor.E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY:
		traversar.toSet = "justify";
		break;
	default:
		throw "EvoEditor.SetAlignment: Unknown alignment value: '" + alignment + "'";
	}

	traversar.record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "setAlignment", null, null, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);

	try {
		EvoEditor.ForeachChildInAffectedContent(affected, traversar);

		if (traversar.record) {
			traversar.record.applyValueAfter = traversar.toSet;
			traversar.record.apply = EvoEditor.applySetAlignment;
		}
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "setAlignment");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);

		if (traversar.anyChanged)
			EvoEditor.EmitContentChanged();
	}
}

EvoEditor.storeAttributes = function(element)
{
	if (!element || !element.attributes.length)
		return null;

	var attributes = [], ii;

	for (ii = 0; ii < element.attributes.length; ii++) {
		var attr = {
			name : element.attributes.item(ii).name,
			value : element.attributes.item(ii).value
		};

		attributes[attributes.length] = attr;
	}

	return attributes;
}

EvoEditor.restoreAttributes = function(element, attributes)
{
	if (!element)
		return;

	while (element.attributes.length) {
		element.removeAttribute(element.attributes.item(element.attributes.length - 1).name);
	}

	if (!attributes)
		return;

	var ii;

	for (ii = 0; ii < attributes.length; ii++) {
		element.setAttribute(attributes[ii].name, attributes[ii].value);
	}
}

EvoEditor.storeElement = function(element)
{
	if (!element)
		return null;

	var elementRecord = {
		tagName : element.tagName,
		attributes : EvoEditor.storeAttributes(element)
	};

	return elementRecord;
}

EvoEditor.restoreElement = function(parentElement, beforeElement, tagName, attributes)
{
	if (!parentElement)
		throw "EvoEditor.restoreElement: parentElement cannot be null";

	if (!tagName)
		throw "EvoEditor.restoreElement: tagName cannot be null";

	var node;

	node = parentElement.ownerDocument.createElement(tagName);

	EvoEditor.restoreAttributes(node, attributes);

	parentElement.insertBefore(node, beforeElement);

	return node;
}

EvoEditor.moveChildren = function(fromElement, toElement, beforeElement, prepareParent, selectionUpdater)
{
	if (!fromElement)
		throw "EvoEditor.moveChildren: fromElement cannot be null";

	if (beforeElement && toElement && !(beforeElement.parentElement === toElement))
		throw "EvoEditor.moveChildren: beforeElement is not a direct child of toElement";

	var node;

	for (node = toElement; node; node = node.parentElement) {
		if (node === fromElement)
			throw "EvoEditor.moveChildren: toElement cannot be child of fromElement";
	}

	var firstElement = toElement;

	while (fromElement.firstChild) {
		if (prepareParent && fromElement.firstChild.tagName && fromElement.firstChild.tagName == "LI") {
			var toParent = prepareParent.exec();

			if (toElement) {
				toElement.parentElement.insertBefore(toParent, toElement.nextElementSibling);
			}

			if (!firstElement) {
				firstElement = toParent;
			}

			var li = fromElement.firstChild, replacedBy = li.firstChild;

			while (li.firstChild) {
				toParent.append(li.firstChild);
			}

			if (selectionUpdater)
				selectionUpdater.beforeRemove(fromElement.firstChild);

			fromElement.removeChild(fromElement.firstChild);

			if (selectionUpdater)
				selectionUpdater.afterRemove(replacedBy);
		} else {
			if (!toElement && prepareParent) {
				toElement = prepareParent.exec();

				// trying to move other than LI into UL/OL, thus do not enclose it into LI
				if (prepareParent.tagName == "LI" && (fromElement.tagName == "UL" || fromElement.tagName == "OL")) {
					var toParent = toElement.parentElement;
					toParent.removeChild(toElement);
					toElement = toParent;
				}
			}

			if (!firstElement) {
				firstElement = toElement;
			}

			toElement.insertBefore(fromElement.firstChild, beforeElement);
		}
	}

	return firstElement;
}

EvoEditor.renameElement = function(element, tagName, attributes, targetElement, selectionUpdater)
{
	var prepareParent = {
		element : element,
		tagName : tagName,
		attributes : attributes,
		targetElement : targetElement,

		exec : function() {
			if (this.targetElement)
				return EvoEditor.restoreElement(this.targetElement, null, this.tagName, this.attributes);
			else
				return EvoEditor.restoreElement(this.element.parentElement, this.element, this.tagName, this.attributes);
		}
	};
	var newElement;

	newElement = EvoEditor.moveChildren(element, null, null, prepareParent, selectionUpdater);

	element.remove();

	return newElement;
}

EvoEditor.getBlockquoteLevel = function(node)
{
	if (!node || node.tagName == "BODY")
		return 0;

	var blockquoteLevel = 0, parent = node;

	while (parent && parent.tagName != "BODY") {
		if (parent.tagName == "BLOCKQUOTE")
			blockquoteLevel++;

		parent = parent.parentElement;
	}

	return blockquoteLevel;
}

EvoEditor.SetBlockFormat = function(format)
{
	var traversar = {
		toSet : null,
		createParent : null,
		firstLI : true,
		targetElement : null,
		selectionUpdater : null,

		flat : true,
		onlyBlockElements : true,

		exec : function(parent, element) {
			var newElement;

			if (this.toSet.tagName != "LI" && (element.tagName == "UL" || element.tagName == "OL")) {
				var affected = [];

				if (!EvoEditor.allChildrenInSelection(element, true, affected)) {
					var elemParent = element.parentElement, insBefore, jj;

					if (affected.length > 0 && !(affected[0] === element.firstElementChild)) {
						insBefore = EvoEditor.splitList(element, 1, affected);
					} else {
						insBefore = element;
					}

					for (jj = 0; jj < affected.length; jj++) {
						EvoEditor.insertListChildBefore(affected[jj], this.toSet.tagName, insBefore ? insBefore.parentElement : elemParent, insBefore, this.selectionUpdater);
					}

					if (!element.childElementCount) {
						this.selectionUpdater.beforeRemove(element);

						element.remove();

						this.selectionUpdater.afterRemove(insBefore ? insBefore.previousElementSibling : elemParent.lastElementChild);
					}

					return true;
				}
			}

			if (this.firstLI) {
				if (this.createParent) {
					this.targetElement = EvoEditor.restoreElement(parent, element, this.createParent.tagName, this.createParent.attributes);
				}

				this.firstLI = false;
			}

			newElement = EvoEditor.renameElement(element, this.toSet.tagName, this.toSet.attributes, this.targetElement, this.selectionUpdater);

			if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && (this.toSet.tagName == "DIV" || this.toSet.tagName == "PRE")) {
				var blockquoteLevel = EvoEditor.getBlockquoteLevel(newElement);

				if (blockquoteLevel > 0) {
					var width = -1;

					if (this.toSet.tagName == "DIV" && blockquoteLevel * 2 < EvoEditor.NORMAL_PARAGRAPH_WIDTH)
						width = EvoEditor.NORMAL_PARAGRAPH_WIDTH - blockquoteLevel * 2;

					EvoEditor.quoteParagraph(newElement, blockquoteLevel, width);
				}
			}

			if (this.selectionUpdater) {
				this.selectionUpdater.beforeRemove(element);
				this.selectionUpdater.afterRemove(newElement);
			}

			return true;
		}
	};

	traversar.selectionUpdater = EvoSelection.CreateUpdaterObject();

	var affected = EvoEditor.ClaimAffectedContent(null, null, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);

	switch (format) {
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH:
		traversar.toSet = { tagName : "DIV" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_PRE:
		traversar.toSet = { tagName : "PRE" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS:
		traversar.toSet = { tagName : "ADDRESS" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H1:
		traversar.toSet = { tagName : "H1" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H2:
		traversar.toSet = { tagName : "H2" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H3:
		traversar.toSet = { tagName : "H3" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H4:
		traversar.toSet = { tagName : "H4" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H5:
		traversar.toSet = { tagName : "H5" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_H6:
		traversar.toSet = { tagName : "H6" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST:
		traversar.toSet = { tagName : "LI" };
		traversar.createParent = { tagName : "UL" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST:
		traversar.toSet = { tagName : "LI" };
		traversar.createParent = { tagName : "OL" };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
		traversar.toSet = { tagName : "LI" };
		traversar.createParent = { tagName : "OL", attributes : [ { name : "type", value : "I" } ] };
		break;
	case EvoEditor.E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
		traversar.toSet = { tagName : "LI" };
		traversar.createParent = { tagName : "OL", attributes : [ { name : "type", value : "A" } ] };
		break;
	default:
		throw "EvoEditor.SetBlockFormat: Unknown block format value: '" + format + "'";
	}

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "setBlockFormat", null, null,
		EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

	try {
		EvoEditor.ForeachChildInAffectedContent(affected, traversar);

		traversar.selectionUpdater.restore();
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "setBlockFormat");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.allChildrenInSelection = function(element, allowPartial, affected)
{
	if (!element || !element.firstChild)
		return false;

	var selection = document.getSelection(), all;

	all = selection.containsNode(element.firstElementChild, allowPartial) &&
	      selection.containsNode(element.lastElementChild, allowPartial);

	var node;

	affected.length = 0;

	for (node = element.firstElementChild; node; node = node.nextElementSibling) {
		if (all || selection.containsNode(node, allowPartial))
			affected[affected.length] = node;
	}

	return all;
}

EvoEditor.splitList = function(element, nParents, onlyAffected)
{
	var parent, from = null;

	if (onlyAffected && onlyAffected.length)
		from = onlyAffected[onlyAffected.length - 1].nextElementSibling;

	if (!from)
		from = element.nextElementSibling;

	if (nParents == -1) {
		nParents = 0;

		for (parent = from; parent && parent.tagName != "BODY"; parent = parent.parentElement) {
			nParents++;
		}
	}

	var nextFrom, clone;

	parent = from ? from.parentElement : element.parentElement;

	if (!from && parent) {
		from = parent.nextElementSibling;
		nextFrom = from;
		nParents--;
		parent = parent.parentElement;
	}

	while (nParents > 0 && parent && parent.tagName != "HTML") {
		nParents--;
		nextFrom = null;

		if (from) {
			clone = from.parentElement.cloneNode(false);
			from.parentElement.parentElement.insertBefore(clone, from.parentElement.nextElementSibling);

			nextFrom = clone;

			while (from.nextElementSibling) {
				clone.appendChild(from.nextElementSibling);
			}

			clone.insertBefore(from, clone.firstElementChild);
		}

		from = nextFrom;
		parent = parent.parentElement;
	}

	if (nextFrom)
		return nextFrom;

	return parent.nextElementSibling;
}

EvoEditor.insertListChildBefore = function(child, tagName, parent, insBefore, selectionUpdater)
{
	if (child.tagName == "LI") {
		var node = document.createElement(tagName);

		while(child.firstChild)
			node.appendChild(child.firstChild);

		parent.insertBefore(node, insBefore);

		if (selectionUpdater)
			selectionUpdater.beforeRemove(child);

		child.remove();

		if (selectionUpdater)
			selectionUpdater.afterRemove(node);
	} else {
		parent.insertBefore(child, insBefore);
	}
}

EvoEditor.applyIndent = function(record, isUndo)
{
	if (record.changes) {
		var ii, parent, child;

		parent = EvoSelection.FindElementByPath(document.body, record.path);
		if (!parent) {
			throw "EvoEditor.applyIndent: Cannot find parent at path " + record.path;
		}

		for (ii = 0; ii < record.changes.length; ii++) {
			var change = record.changes[isUndo ? (record.changes.length - ii - 1) : ii];

			child = EvoSelection.FindElementByPath(change.pathIsFromBody ? document.body : parent, change.path);
			if (!child) {
				throw "EvoEditor.applyIndent: Cannot find child";
			}

			if (change.isList) {
				EvoUndoRedo.RestoreChildren(change, child, isUndo);
				continue;
			}

			if (isUndo) {
				child.style.marginLeft = change.beforeMarginLeft;
				child.style.marginRight = change.beforeMarginRight;
				child.style.width = change.beforeWidth;
			} else {
				child.style.marginLeft = change.afterMarginLeft;
				child.style.marginRight = change.afterMarginRight;
				child.style.width = change.afterWidth;
			}

			EvoEditor.removeEmptyStyleAttribute(child);
		}
	}
}

EvoEditor.Indent = function(increment)
{
	var traversar = {
		record : null,
		selectionUpdater : null,
		increment : increment,

		flat : true,
		onlyBlockElements : true,

		exec : function(parent, element) {
			var change = null, isList = element.tagName == "UL" || element.tagName == "OL";
			var isNested = isList && (element.parentElement.tagName == "UL" || element.parentElement.tagName == "OL");

			if (traversar.record) {
				if (!traversar.record.changes)
					traversar.record.changes = [];

				change = {};

				change.pathIsFromBody = false;

				if (isList) {
					change.isList = isList;
					change.path = EvoSelection.GetChildPath(parent, element);
				} else {
					change.path = EvoSelection.GetChildPath(parent, element);
					change.beforeMarginLeft = element.style.marginLeft;
					change.beforeMarginRight = element.style.marginRight;
					change.beforeWidth = element.style.width;
				}

				traversar.record.changes[traversar.record.changes.length] = change;
			}

			if (isList) {
				var elemParent = null, all, affected = [], jj;

				all = EvoEditor.allChildrenInSelection(element, true, affected);

				if (this.increment) {
					var clone;

					clone = element.cloneNode(false);

					if (all) {
						if (change) {
							var childIndex = EvoEditor.GetChildIndex(element.parentElement, element);
							EvoUndoRedo.BackupChildrenBefore(change, element.parentElement, childIndex, childIndex);
							change.path = EvoSelection.GetChildPath(parent, element.parentElement);
						}

						element.parentElement.insertBefore(clone, element);
						clone.appendChild(element);

						if (change)
							EvoUndoRedo.BackupChildrenAfter(change, clone.parentElement);
					} else if (affected.length > 0) {
						if (change) {
							EvoUndoRedo.BackupChildrenBefore(change, element,
								EvoEditor.GetChildIndex(element, affected[0]),
								EvoEditor.GetChildIndex(element, affected[affected.length - 1]));
						}

						element.insertBefore(clone, affected[0]);

						for (jj = 0; jj < affected.length; jj++) {
							clone.appendChild(affected[jj]);
						}

						if (change)
							EvoUndoRedo.BackupChildrenAfter(change, element);
					}
				} else {
					var insBefore = null;

					elemParent = element.parentElement;

					// decrease indent in nested lists of the same type will merge items into one list
					if (isNested && elemParent.tagName == element.tagName &&
					    elemParent.getAttribute("type") == element.getAttribute("type")) {
						if (change) {
							var childIndex = EvoEditor.GetChildIndex(elemParent, element);
							EvoUndoRedo.BackupChildrenBefore(change, elemParent, childIndex, childIndex);
							change.path = EvoSelection.GetChildPath(parent, elemParent);
						}

						if (!all && affected.length > 0 && !(affected[0] === element.firstElementChild)) {
							insBefore = EvoEditor.splitList(element, 1, affected);
						} else {
							insBefore = element;
						}

						for (jj = 0; jj < affected.length; jj++) {
							elemParent.insertBefore(affected[jj], insBefore);
						}

						if (!element.childElementCount) {
							this.selectionUpdater.beforeRemove(element);

							element.remove();

							this.selectionUpdater.afterRemove(affected[0]);
						}

						if (change)
							EvoUndoRedo.BackupChildrenAfter(change, elemParent);
					} else {
						var tmpElement = element;

						if (isNested) {
							tmpElement = elemParent;
							elemParent = elemParent.parentElement;
						}

						if (change) {
							var childIndex = EvoEditor.GetChildIndex(elemParent, tmpElement);
							EvoUndoRedo.BackupChildrenBefore(change, elemParent, childIndex, childIndex);
							if (isNested) {
								change.pathIsFromBody = true;
								change.path = EvoSelection.GetChildPath(document.body, elemParent);
							} else {
								change.path = EvoSelection.GetChildPath(parent, elemParent);
							}
						}

						if (isNested) {
							var clone;

							insBefore = EvoEditor.splitList(element, 1, affected);

							clone = element.cloneNode(false);
							if (insBefore)
								insBefore.parentElement.insertBefore(clone, insBefore);
							else
								elemParent.insertBefore(clone, insBefore);

							for (jj = 0; jj < affected.length; jj++) {
								clone.appendChild(affected[jj]);
							}
						} else {
							if (!all && affected.length > 0 && affected[affected.length - 1] === element.lastElementChild) {
								insBefore = element.nextElementSibling;
							} else if (!all && affected.length > 0 && !(affected[0] === element.firstElementChild)) {
								insBefore = EvoEditor.splitList(element, 1, affected);
							} else {
								insBefore = element;
							}

							for (jj = 0; jj < affected.length; jj++) {
								EvoEditor.insertListChildBefore(affected[jj], "DIV", insBefore ? insBefore.parentElement : elemParent, insBefore, this.selectionUpdater);
							}
						}

						while (element && !(element === elemParent) && !element.childElementCount) {
							tmpElement = element.parentElement;

							this.selectionUpdater.beforeRemove(element);

							element.remove();

							this.selectionUpdater.afterRemove(insBefore ? insBefore.previousElementSibling : elemParent.lastElementChild);

							element = tmpElement;
						}

						if (change)
							EvoUndoRedo.BackupChildrenAfter(change, elemParent);
					}
				}
			} else {
				var currValue = null, dir, width;

				dir = window.getComputedStyle(element).direction;

				if (dir == "rtl") {
					if (element.style.marginRight.endsWith("ch"))
						currValue = element.style.marginRight;
				} else { // "ltr" or other
					if (element.style.marginLeft.endsWith("ch"))
						currValue = element.style.marginLeft;
				}

				if (!currValue) {
					currValue = 0;
				} else {
					currValue = parseInt(currValue.slice(0, -2));
					if (!Number.isInteger(currValue))
						currValue = 0;
				}

				width = 0;
				if (element.style.width.endsWith("ch")) {
					width = parseInt(element.style.width.slice(0, -2));
					if (!Number.isInteger(width))
						width = 0;
				}

				if (traversar.increment) {
					if (width && width - EvoEditor.TEXT_INDENT_SIZE > 0)
						width = width - EvoEditor.TEXT_INDENT_SIZE;
					currValue = (currValue + EvoEditor.TEXT_INDENT_SIZE) + "ch";
				} else if (currValue > EvoEditor.TEXT_INDENT_SIZE) {
					if (width)
						width = width + EvoEditor.TEXT_INDENT_SIZE;
					currValue = (currValue - EvoEditor.TEXT_INDENT_SIZE) + "ch";
				} else {
					if (width)
						width = width + currValue;
					currValue = "";
				}

				if (dir == "rtl") {
					element.style.marginRight = currValue;
				} else { // "ltr" or other
					element.style.marginLeft = currValue;
				}

				if (width)
					element.style.width = width + "ch";

				if (change) {
					change.afterMarginLeft = element.style.marginLeft;
					change.afterMarginRight = element.style.marginRight;
					change.afterWidth = element.style.width;
				}

				EvoEditor.removeEmptyStyleAttribute(element);
			}

			return true;
		}
	};

	var affected = EvoEditor.ClaimAffectedContent(null, null, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);

	traversar.record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, increment ? "Indent" : "Outdent", null, null, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);
	traversar.selectionUpdater = EvoSelection.CreateUpdaterObject();

	try {
		EvoEditor.ForeachChildInAffectedContent(affected, traversar);

		if (traversar.record) {
			traversar.record.apply = EvoEditor.applyIndent;
		}

		traversar.selectionUpdater.restore();
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, increment ? "Indent" : "Outdent");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.applyDivNormalize = function(record, isUndo)
{
	var element = EvoSelection.FindElementByPath(document.body, record.path);

	if (!element)
		throw "EvoEditor.applyDivNormalize: Path not found";

	var value;

	if (isUndo)
		value = record.beforeValue;
	else
		value = record.afterValue;

	if (record.isWidthStyle) {
		element.style.width = value;
		EvoEditor.removeEmptyStyleAttribute(element);
	} else {
		element.innerHTML = value;
	}
}

EvoEditor.correctParagraphsAfterInsertContent = function(opType)
{
	var node, list, ii;

	list = document.body.getElementsByTagName("DIV");

	for (ii = 0; ii < list.length; ii++) {
		node = list[ii];

		if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
			var beforeValue = node.style.width;
			EvoEditor.maybeUpdateParagraphWidth(node);

			if (node.style.width != beforeValue) {
				var record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType + "::divWidths", node, node, EvoEditor.CLAIM_CONTENT_FLAG_NONE);
				try {
					if (record) {
						record.path = EvoSelection.GetChildPath(document.body, node);
						record.beforeValue = beforeValue;
						record.afterValue = node.style.width;
						record.isWidthStyle = true;
						record.apply = EvoEditor.applyDivNormalize;
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType + "::divWidths");
				}
			}
		}

		if (!node.firstChild) {
			var record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType + "::divBR", node, node, EvoEditor.CLAIM_CONTENT_FLAG_NONE);
			try {
				beforeValue = node.innerHTML;
				node.appendChild(document.createElement("BR"));

				if (record) {
					record.path = EvoSelection.GetChildPath(document.body, node);
					record.beforeValue = beforeValue;
					record.afterValue = node.innerHTML;
					record.apply = EvoEditor.applyDivNormalize;
				}
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType + "::divBR");
			}
		}
	}
}

EvoEditor.InsertHTML = function(opType, html)
{
	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, opType);
	try {
		document.execCommand("insertHTML", false, html);

		var node, list, ii;

		list = document.body.getElementsByTagName("BLOCKQUOTE");

		for (ii = 0; ii < list.length; ii++) {
			node = list[ii];

			EvoEditor.setAttributeWithUndoRedo("InsertHTML::fixBlockquote", node, "class", null);
			EvoEditor.setAttributeWithUndoRedo("InsertHTML::fixBlockquote", node, "style", null);
		}

		EvoEditor.correctParagraphsAfterInsertContent(opType);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, opType);
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.InsertText = function(opType, text)
{
	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, opType);
	try {
		document.execCommand("insertText", false, text);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, opType);
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.SetBodyAttribute = function(name, value)
{
	EvoEditor.setAttributeWithUndoRedo("SetBodyAttribute", document.body, name, value);
}

EvoEditor.applySetBodyFontName = function(record, isUndo)
{
	EvoEditor.UpdateStyleSheet("x-evo-body-fontname", isUndo ? record.beforeCSS : record.afterCSS);

	if (record.beforeStyle != record.afterStyle) {
		document.body.style.fontFamily = isUndo ? record.beforeStyle : record.afterStyle;
		EvoEditor.removeEmptyStyleAttribute(body.document);
	}
}

EvoEditor.SetBodyFontName = function(name)
{
	var record;

	record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "setBodyFontName", document.body, document.body, EvoEditor.CLAIM_CONTENT_FLAG_NONE);

	try {
		var beforeCSS, css, beforeStyle;

		if (name)
			css = "body { font-family: " + name + "; }";
		else
			css = null;

		beforeStyle = document.body.style.fontFamily;
		beforeCSS = EvoEditor.UpdateStyleSheet("x-evo-body-fontname", css);

		if (name != document.body.style.fontFamily)
			document.body.style.fontFamily = name ? name : "";

		if (record) {
			record.apply = EvoEditor.applySetBodyFontName;
			record.beforeCSS = beforeCSS;
			record.afterCSS = css;
			record.beforeStyle = beforeStyle;
			record.afterStyle = document.body.style.fontFamily;

			if (record.beforeCSS == record.afterCSS && record.beforeStyle == record.afterStyle)
				record.ignore = true;
		}

		EvoEditor.removeEmptyStyleAttribute(document.body);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "setBodyFontName");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_YES);

		if (!record || !record.ignore)
			EvoEditor.EmitContentChanged();
	}
}

EvoEditor.emptyParagraphAsHtml = function()
{
	if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
		return "<div style=\"width:" + EvoEditor.NORMAL_PARAGRAPH_WIDTH + "ch;\"><br></div>";
	} else {
		return "<div><br></div>";
	}
}

EvoEditor.initializeContent = function()
{
	// for backward compatibility
	document.execCommand("StyleWithCSS", false, "false");

	if (document.body) {
		// attach on body, thus it runs before EvoUndoRedo.beforeInputCb()
		document.body.onbeforeinput = EvoEditor.beforeInputCb;

		if (!document.body.firstChild) {
			EvoUndoRedo.Disable();
			try {
				document.body.innerHTML = EvoEditor.emptyParagraphAsHtml();
			} finally {
				EvoUndoRedo.Enable();
			}
		}

		// make sure there is a selection
		if (!document.getSelection().anchorNode || document.getSelection().anchorNode.tagName == "HTML") {
			document.getSelection().setPosition(document.body.firstChild ? document.body.firstChild : document.body, 0);
		}
	}
}

EvoEditor.getNextNodeInHierarchy = function(node, upToNode)
{
	if (!node)
		return null;

	var next;

	next = node.firstChild;

	if (!next)
		next = node.nextSibling;

	if (!next) {
		next = node.parentElement;

		if (next === upToNode || next === document.body)
			next = null;

		while (next) {
			if (next.nextSibling) {
				next = next.nextSibling;
				break;
			} else {
				next = next.parentElement;

				if (next === upToNode || next === document.body)
					next = null;
			}
		}
	}

	return next;
}

// it already knows the line is too long; the node is where the text length exceeded
EvoEditor.quoteParagraphWrap = function(node, lineLength, wrapWidth, prefixHtml)
{
	if (node.nodeType == node.ELEMENT_NODE) {
		var br = document.createElement("BR");
		br.className = "-x-evo-wrap-br";

		node.insertAdjacentElement("beforebegin", br);
		node.insertAdjacentHTML("beforebegin", prefixHtml);

		return node.innerText.length;
	}

	var words = node.nodeValue.split(" "), ii, offset = 0, inc;

	for (ii = 0; ii < words.length; ii++) {
		var word = words[ii], wordLen = word.length;

		if (lineLength + wordLen > wrapWidth) {
			if (offset > 0) {
				node.splitText(offset - 1);
				node = node.nextSibling;

				// erase the space at the end of the line
				node.splitText(1);
				var next = node.nextSibling;
				node.remove();
				node = next;

				var br = document.createElement("BR");
				br.className = "-x-evo-wrap-br";

				node.parentElement.insertBefore(br, node);
				br.insertAdjacentHTML("afterend", prefixHtml);
			}

			offset = 0;
			lineLength = 0;
		}

		inc = wordLen + (ii + 1 < words.length ? 1 : 0);

		lineLength += inc;
		offset += inc;
	}

	return lineLength;
}

EvoEditor.getBlockquotePrefixHtml = function(blockquoteLevel)
{
	var prefixHtml;

	prefixHtml = "<span class='-x-evo-quote-character'>&gt; </span>".repeat(blockquoteLevel);
	prefixHtml = "<span class='-x-evo-quoted'>" + prefixHtml + "</span>";

	return prefixHtml;
}

EvoEditor.quoteParagraph = function(paragraph, blockquoteLevel, wrapWidth)
{
	if (!paragraph || !(blockquoteLevel > 0))
		return;

	EvoEditor.removeQuoteMarks(paragraph);

	if (paragraph.tagName == "PRE")
		wrapWidth = -1;

	var node = paragraph.firstChild, next, lineLength = 0;
	var prefixHtml = EvoEditor.getBlockquotePrefixHtml(blockquoteLevel);

	while (node) {
		next = EvoEditor.getNextNodeInHierarchy(node, paragraph);

		if (node.nodeType == node.TEXT_NODE) {
			if (wrapWidth > 0 && lineLength + node.nodeValue.length > wrapWidth) {
				lineLength = EvoEditor.quoteParagraphWrap(node, lineLength, wrapWidth, prefixHtml);
			} else {
				lineLength += node.nodeValue.length;
			}
		} else if (node.nodeType == node.ELEMENT_NODE) {
			if (node.tagName == "BR") {
				if (node.classList.contains("-x-evo-wrap-br")) {
					node.remove();
				} else {
					if (node.parentElement.childNodes.length != 1)
						node.insertAdjacentHTML("afterend", prefixHtml);

					lineLength = 0;
				}
			} else if (node.tagName == "A") {
				var len = node.innerText.length;

				if (wrapWidth > 0 && lineLength + len > wrapWidth && (!next || next.tagName != "BR")) {
					lineLength = EvoEditor.quoteParagraphWrap(node, lineLength, wrapWidth, prefixHtml);
				} else {
					lineLength += len;
				}

				// do not traverse into the anchor element
				next = node.nextSibling;
			}
		}

		node = next;
	}

	paragraph.insertAdjacentHTML("afterbegin", prefixHtml);
}

EvoEditor.reBlockquotePlainText = function(plainText, usePreTag)
{
	var lines = plainText.replace(/\&/g, "&amp;").split("\n"), ii, html = "", level = 0;

	for (ii = 0; ii < lines.length; ii++) {
		var line = lines[ii], newLevel = 0, skip = 0;

		// Conversion to Plain Text adds empty line at the end
		if (ii + 1 >= lines.length && !line[0])
			break;

		while (line[skip] == '>') {
			newLevel++;
			skip++;
			if (line[skip] == ' ')
				skip++;
		}

		while (newLevel > level) {
			html += "<blockquote type='cite'>";
			level++;
		}

		while (newLevel < level) {
			html += "</blockquote>";
			level--;
		}

		html += usePreTag ? "<pre>" : "<div>";

		while (line[skip] == ' ') {
			skip++;
			html += "&nbsp;";
		}

		if (skip)
			line = line.substr(skip);

		html += (line[0] ? line.replace(/</g, "&lt;").replace(/>/g, "&gt;") : "<br>");
		html += usePreTag ? "</pre>" : "</div>";
	}

	while (0 < level) {
		html += "</blockquote>";
		level--;
	}

	return html;
}

EvoEditor.convertParagraphs = function(parent, blockquoteLevel, wrapWidth, canChangeQuoteParagraphs)
{
	if (!parent)
		return;

	var ii;

	for (ii = 0; ii < parent.children.length; ii++) {
		var child = parent.children.item(ii);

		if (child.tagName == "DIV") {
			if (wrapWidth == -1 || (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && blockquoteLevel > 0)) {
				child.style.width = "";
				EvoEditor.removeEmptyStyleAttribute(child);
			} else {
				child.style.width = wrapWidth + "ch";
				child.removeAttribute("x-evo-width");
			}

			if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && blockquoteLevel > 0)
				EvoEditor.quoteParagraph(child, blockquoteLevel, wrapWidth);
		} else if (child.tagName == "PRE") {
			if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && blockquoteLevel > 0) {
				var prefixHtml = EvoEditor.getBlockquotePrefixHtml(blockquoteLevel);
				var lines, jj, text;

				text = child.innerText;

				if (text == "\n" || text == "\r\n")
					lines = [ "" ];
				else
					lines = text.split("\n");

				text = "";

				for (jj = 0; jj < lines.length; jj++) {
					text += prefixHtml + lines[jj].replace(/\&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

					if (!lines[jj])
						text += "<BR>";

					if (jj + 1 < lines.length)
						text += "\n";
				}

				if (!lines.length)
					text += prefixHtml;

				child.innerHTML = text;
			} else {
				EvoEditor.convertParagraphs(child, blockquoteLevel, wrapWidth, canChangeQuoteParagraphs);
			}
		} else if (child.tagName == "BLOCKQUOTE") {
			var innerWrapWidth = wrapWidth;

			if (innerWrapWidth > 0) {
				innerWrapWidth -= 2; // length of "> "

				if (innerWrapWidth < EvoConvert.MIN_PARAGRAPH_WIDTH)
					innerWrapWidth = EvoConvert.MIN_PARAGRAPH_WIDTH;
			}

			// replace blockquote content with pure plain text and then re-blockquote it
			// and do it only on the top level, not recursively (nested citations)
			if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && !blockquoteLevel) {
				child.innerHTML = EvoEditor.reBlockquotePlainText(EvoConvert.ToPlainText(child, -1),
					(child.firstElementChild && child.firstElementChild.tagName == "PRE" && (
					!canChangeQuoteParagraphs || !EvoEditor.WRAP_QUOTED_TEXT_IN_REPLIES)));
			}

			EvoEditor.convertParagraphs(child, blockquoteLevel + 1, innerWrapWidth, canChangeQuoteParagraphs);
		} else if (child.tagName == "UL") {
			if (wrapWidth == -1) {
				child.style.width = "";
				EvoEditor.removeEmptyStyleAttribute(child);
			} else {
				var innerWrapWidth = wrapWidth;

				innerWrapWidth -= 3; // length of " * " prefix

				if (innerWrapWidth < EvoConvert.MIN_PARAGRAPH_WIDTH)
					innerWrapWidth = EvoConvert.MIN_PARAGRAPH_WIDTH;

				child.style.width = innerWrapWidth + "ch";
			}
		} else if (child.tagName == "OL") {
			if (wrapWidth == -1) {
				child.style.width = "";
				child.style.paddingInlineStart = "";
				EvoEditor.removeEmptyStyleAttribute(child);
			} else {
				var innerWrapWidth = wrapWidth, olNeedWidth;

				olNeedWidth = EvoConvert.GetOLMaxLetters(child.getAttribute("type"), child.children.length) + 2; // length of ". " suffix

				if (olNeedWidth < EvoConvert.MIN_OL_WIDTH)
					olNeedWidth = EvoConvert.MIN_OL_WIDTH;

				innerWrapWidth -= olNeedWidth;

				if (innerWrapWidth < EvoConvert.MIN_PARAGRAPH_WIDTH)
					innerWrapWidth = EvoConvert.MIN_PARAGRAPH_WIDTH;

				child.style.width = innerWrapWidth + "ch";
				child.style.paddingInlineStart = olNeedWidth + "ch";
			}
		}
	}
}

EvoEditor.SetNormalParagraphWidth = function(value)
{
	if (EvoEditor.NORMAL_PARAGRAPH_WIDTH != value) {
		EvoEditor.NORMAL_PARAGRAPH_WIDTH = value;

		if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT)
			EvoEditor.convertParagraphs(document.body, 0, EvoEditor.NORMAL_PARAGRAPH_WIDTH, false);
	}
}

EvoEditor.moveNodeContent = function(node, intoNode)
{
	if (!node || !node.parentElement)
		return;

	var parent = node.parentElement;

	while (node.firstChild) {
		if (intoNode) {
			intoNode.append(node.firstChild);
		} else {
			parent.insertBefore(node.firstChild, node);
		}
	}
}

EvoEditor.convertTags = function()
{
	var ii, list;

	for (ii = document.images.length - 1; ii >= 0; ii--) {
		var img = document.images[ii];

		img.outerText = EvoConvert.ImgToText(img);
	}

	list = document.getElementsByTagName("A");

	for (ii = list.length - 1; ii >= 0; ii--) {
		var anchor = list[ii];

		EvoEditor.moveNodeContent(anchor);

		anchor.remove();
	}

	list = document.getElementsByTagName("TABLE");

	for (ii = list.length - 1; ii >= 0; ii--) {
		var table = list[ii], textNode;

		textNode = document.createTextNode(table.innerText);
		table.parentElement.insertBefore(textNode, table);
		table.remove();
	}

	list = document.getElementsByTagName("BLOCKQUOTE");

	for (ii = list.length - 1; ii >= 0; ii--) {
		var blockquoteNode = list[ii];

		blockquoteNode.removeAttribute("class");
		blockquoteNode.removeAttribute("style");
	}

	var node = document.body.firstChild, next;

	while (node) {
		var removeNode = false;

		if (node.nodeType == node.ELEMENT_NODE) {
			if (node.tagName != "DIV" &&
			    node.tagName != "PRE" &&
			    node.tagName != "BLOCKQUOTE" &&
			    node.tagName != "UL" &&
			    node.tagName != "OL" &&
			    node.tagName != "LI" &&
			    node.tagName != "BR") {
				removeNode = true;

				// convert P into DIV
				if (node.tagName == "P") {
					var div = document.createElement("DIV");
					EvoEditor.moveNodeContent(node, div);
					node.parentElement.insertBefore(div, node.nextSibling);
				} else {
					EvoEditor.moveNodeContent(node);
				}
			}
		}

		// skip the signature wrapper
		if (!removeNode && node.tagName == "DIV" && node.className == "-x-evo-signature-wrapper")
			next = node.nextSibling;
		else
			next = EvoEditor.getNextNodeInHierarchy(node, document.body);

		if (removeNode)
			node.remove();

		node = next;
	}

	document.body.normalize();
}

EvoEditor.removeQuoteMarks = function(element)
{
	var ii, list;

	if (!element)
		element = document;

	list = element.querySelectorAll("SPAN.-x-evo-quoted");

	for (ii = list.length - 1; ii >= 0; ii--) {
		var node = list[ii];

		node.remove();
	}

	list = element.querySelectorAll("BR.-x-evo-wrap-br");

	for (ii = list.length - 1; ii >= 0; ii--) {
		var node = list[ii];

		node.insertAdjacentText("beforebegin", " ");
		node.remove();
	}

	if (element === document)
		document.body.normalize();
	else
		element.normalize();
}

EvoEditor.SetMode = function(mode)
{
	if (EvoEditor.mode != mode) {
		var opType = "setMode::" + (mode == EvoEditor.MODE_PLAIN_TEXT ? "PlainText" : "HTML"), record;

		record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_DOCUMENT, opType, null, null);

		if (record) {
			record.modeBefore = EvoEditor.mode;
			record.modeAfter = mode;
			record.apply = function(record, isUndo) {
				var useMode = isUndo ? record.modeBefore : record.modeAfter;

				if (EvoEditor.mode != useMode) {
					EvoEditor.mode = useMode;
				}
			}
		}

		EvoUndoRedo.Disable();
		try {
			EvoEditor.mode = mode;

			EvoEditor.removeQuoteMarks(null);

			if (mode == EvoEditor.MODE_PLAIN_TEXT) {
				EvoEditor.convertTags();
				EvoEditor.convertParagraphs(document.body, 0, EvoEditor.NORMAL_PARAGRAPH_WIDTH, false);
			} else {
				EvoEditor.convertParagraphs(document.body, 0, -1, false);
			}
		} finally {
			EvoUndoRedo.Enable();
			EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_DOCUMENT, opType);

			EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_YES);
		}
	}
}

EvoEditor.applyFontReset = function(record, isUndo)
{
	if (record.changes) {
		var ii;

		for (ii = 0; ii < record.changes.length; ii++) {
			var change = record.changes[isUndo ? (record.changes.length - ii - 1) : ii];
			var parent = EvoSelection.FindElementByPath(document.body, change.parentPath);

			if (!parent) {
				throw "EvoEditor.applyFontReset: Cannot find node at path " + change.path;
			}

			parent.innerHTML = isUndo ? change.htmlBefore : change.htmlAfter;
		}
	}
}

EvoEditor.replaceInheritFonts = function(undoRedoRecord, selectionUpdater, nodes)
{
	var ii;

	if (!nodes)
		nodes = document.querySelectorAll("FONT[face=inherit]");

	for (ii = nodes.length - 1; ii >= 0; ii--) {
		var node = nodes.item(ii);

		if (!node || (!undoRedoRecord && !document.getSelection().containsNode(node, true)))
			continue;

		var parent, change = null;

		parent = node.parentElement;

		if (undoRedoRecord) {
			if (!undoRedoRecord.changes)
				undoRedoRecord.changes = [];

			change = {
				parentPath : EvoSelection.GetChildPath(document.body, parent),
				htmlBefore : parent.innerHTML,
				htmlAfter : ""
			};

			undoRedoRecord.changes[undoRedoRecord.changes.length] = change;
		}

		if (node.attributes.length == 1) {
			var child;

			while (node.firstChild) {
				var child = node.firstChild;

				selectionUpdater.beforeRemove(child);

				parent.insertBefore(child, node);

				selectionUpdater.afterRemove(child);
			}

			node.remove();
		} else {
			node.removeAttribute("face");
		}

		if (change)
			change.htmlAfter = parent.innerHTML;
	}

	if (undoRedoRecord && undoRedoRecord.changes)
		undoRedoRecord.apply = EvoEditor.applyFontReset;
}

EvoEditor.maybeReplaceInheritFonts = function()
{
	var nodes = document.querySelectorAll("FONT[face=inherit]");

	if (nodes.length <= 0)
		return;

	var record, selectionUpdater;

	selectionUpdater = EvoSelection.CreateUpdaterObject();

	record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "UnsetFontName", null, null, EvoEditor.CLAIM_CONTENT_FLAG_NONE);
	try {
		EvoEditor.replaceInheritFonts(record, selectionUpdater, nodes);

		selectionUpdater.restore();
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "UnsetFontName");

		if (record)
			EvoUndoRedo.GroupTopRecords(2);
	}
}

EvoEditor.SetFontName = function(name)
{
	if (!name || name == "")
		name = "inherit";

	var record, selectionUpdater = EvoSelection.CreateUpdaterObject(), bodyFontFamily;

	// to workaround https://bugs.webkit.org/show_bug.cgi?id=204622
	bodyFontFamily = document.body.style.fontFamily;

	record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "SetFontName");
	try {
		if (!document.getSelection().isCollapsed && bodyFontFamily)
			document.body.style.fontFamily = "";

		document.execCommand("FontName", false, name);

		if (document.getSelection().isCollapsed) {
			if (name == "inherit")
				EvoEditor.checkInheritFontsOnChange = true;

			/* Format change on collapsed selection is not applied immediately */
			if (record)
				record.ignore = true;
		} else if (name == "inherit") {
			var subrecord;

			subrecord = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "SetFontName", null, null, EvoEditor.CLAIM_CONTENT_FLAG_NONE);
			try {
				EvoEditor.replaceInheritFonts(subrecord, selectionUpdater);
				selectionUpdater.restore();
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "SetFontName");
			}
		}
	} finally {
		if (bodyFontFamily && document.body.style.fontFamily != bodyFontFamily)
			document.body.style.fontFamily = bodyFontFamily;

		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, "SetFontName");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();

		EvoEditor.removeEmptyStyleAttribute(document.body);
	}
}

EvoEditor.convertHtmlToSend = function()
{
	var html, bgcolor, text, link, vlink;
	var unsetBgcolor = false, unsetText = false, unsetLink = false, unsetVlink = false;
	var themeCss, inheritThemeColors = EvoEditor.inheritThemeColors;
	var ii, styles, styleNode = null, topSignatureSpacers, elems;

	themeCss = EvoEditor.UpdateThemeStyleSheet(null);
	bgcolor = document.documentElement.getAttribute("x-evo-bgcolor");
	text = document.documentElement.getAttribute("x-evo-text");
	link = document.documentElement.getAttribute("x-evo-link");
	vlink = document.documentElement.getAttribute("x-evo-vlink");

	document.documentElement.removeAttribute("x-evo-bgcolor");
	document.documentElement.removeAttribute("x-evo-text");
	document.documentElement.removeAttribute("x-evo-link");
	document.documentElement.removeAttribute("x-evo-vlink");

	topSignatureSpacers = document.querySelectorAll(".-x-evo-top-signature-spacer");

	for (ii = topSignatureSpacers.length - 1; ii >= 0; ii--) {
		topSignatureSpacers[ii].removeAttribute("class");
	}

	if (inheritThemeColors) {
		if (bgcolor && !document.body.getAttribute("bgcolor")) {
			document.body.setAttribute("bgcolor", bgcolor);
			unsetBgcolor = true;
		}

		if (text && !document.body.getAttribute("text")) {
			document.body.setAttribute("text", text);
			unsetText = true;
		}

		if (link && !document.body.getAttribute("link")) {
			document.body.setAttribute("link", link);
			unsetLink = true;
		}

		if (vlink && !document.body.getAttribute("vlink")) {
			document.body.setAttribute("vlink", vlink);
			unsetVlink = true;
		}
	}

	styles = document.head.getElementsByTagName("style");

	for (ii = 0; ii < styles.length; ii++) {
		if (styles[ii].id == "x-evo-body-fontname") {
			styleNode = styles[ii];
			styleNode.id = "";
			break;
		}
	}

	if (EvoEditor.mode == EvoEditor.MODE_HTML) {
		elems = document.getElementsByTagName("BLOCKQUOTE");

		for (ii = 0; ii < elems.length; ii++) {
			elems[ii].setAttribute("style", EvoEditor.BLOCKQUOTE_STYLE);
		}
	}

	html = document.documentElement.outerHTML;

	if (EvoEditor.mode == EvoEditor.MODE_HTML) {
		elems = document.getElementsByTagName("BLOCKQUOTE");

		for (ii = 0; ii < elems.length; ii++) {
			elems[ii].removeAttribute("style");
		}
	}

	if (styleNode)
		styleNode.id = "x-evo-body-fontname";

	if (bgcolor)
		document.documentElement.setAttribute("x-evo-bgcolor", bgcolor);
	if (text)
		document.documentElement.setAttribute("x-evo-text", text);
	if (link)
		document.documentElement.setAttribute("x-evo-link", link);
	if (vlink)
		document.documentElement.setAttribute("x-evo-vlink", vlink);

	if (inheritThemeColors) {
		if (unsetBgcolor)
			document.body.removeAttribute("bgcolor");

		if (unsetText)
			document.body.removeAttribute("text");

		if (unsetLink)
			document.body.removeAttribute("link");

		if (unsetVlink)
			document.body.removeAttribute("vlink");
	}

	for (ii = topSignatureSpacers.length - 1; ii >= 0; ii--) {
		var elem = topSignatureSpacers[ii];

		if (elem.previousSibling && elem.previousSibling.tagName == "DIV" && elem.previousSibling.className == "-x-evo-signature-wrapper") {
			elem.className = "-x-evo-top-signature-spacer";
			break;
		}
	}

	if (themeCss)
		EvoEditor.UpdateThemeStyleSheet(themeCss);

	return html;
}

EvoEditor.GetContent = function(flags, cid_uid_prefix)
{
	var content_data = {};

	if (!document.body)
		return content_data;

	var img_elems = [], data_names = [], bkg_elems = [], elems, ii, jj, currentElemsArray = null;
	var scrollX = window.scrollX, scrollY = window.scrollY;

	EvoUndoRedo.Disable();

	try {
		currentElemsArray = EvoEditor.RemoveCurrentElementAttr();

		// For safety, to not export with empty 'style' attributes; these do not need Undo
		elems = document.querySelectorAll("[style='']");
		for (ii = 0; ii < elems.length; ii++) {
			EvoEditor.removeEmptyStyleAttribute(elems[ii]);
		}

		if ((flags & EvoEditor.E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED) != 0) {
			var hidden_elems = [];

			try {
				elems = document.getElementsByClassName("-x-evo-signature-wrapper");
				if (elems && elems.length) {
					for (ii = 0; ii < elems.length; ii++) {
						var elem = elems.item(ii);

						if (elem && !elem.hidden) {
							hidden_elems[hidden_elems.length] = elem;
							elem.hidden = true;
						}
					}
				}

				elems = document.getElementsByTagName("BLOCKQUOTE");
				if (elems && elems.length) {
					for (ii = 0; ii < elems.length; ii++) {
						var elem = elems.item(ii);

						if (elem && !elem.hidden) {
							hidden_elems[hidden_elems.length] = elem;
							elem.hidden = true;
						}
					}
				}

				content_data["raw-body-stripped"] = document.body.innerText;
			} finally {
				for (ii = 0; ii < hidden_elems.length; ii++) {
					hidden_elems[ii].hidden = false;
				}
			}
		}

		// Do these before changing image sources
		if ((flags & EvoEditor.E_CONTENT_EDITOR_GET_RAW_BODY_HTML) != 0)
			content_data["raw-body-html"] = document.body.innerHTML;

		if ((flags & EvoEditor.E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN) != 0)
			content_data["raw-body-plain"] = document.body.innerText;

		if (EvoEditor.mode == EvoEditor.MODE_HTML &&
		    (flags & EvoEditor.E_CONTENT_EDITOR_GET_INLINE_IMAGES) != 0) {
			var images = [];

			for (ii = 0; ii < document.images.length; ii++) {
				var elem = document.images.item(ii);
				var src = (elem && elem.src) ? elem.src.toLowerCase() : "";

				if (elem && (
				    src.startsWith("data:") ||
				    src.startsWith("file://") ||
				    src.startsWith("evo-file://"))) {
					for (jj = 0; jj < img_elems.length; jj++) {
						if (elem.src == img_elems[jj].orig_src) {
							img_elems[jj].subelems[img_elems[jj].subelems.length] = elem;
							elem.src = img_elems[jj].cid;
							break;
						}
					}

					if (jj >= img_elems.length) {
						var img_obj = {
							subelems : [ elem ],
							cid : "cid:" + cid_uid_prefix + "-" + img_elems.length,
							orig_src : elem.src
						};

						if (elem.src.toLowerCase().startsWith("cid:"))
							img_obj.cid = elem.src;

						img_elems[img_elems.length] = img_obj;
						images[images.length] = {
							cid : img_obj.cid,
							src : elem.src
						};
						elem.src = img_obj.cid;

						if (elem.hasAttribute("data-name"))
							images[images.length - 1].name = elem.getAttribute("data-name");
					}
				} else if (elem && src.startsWith("cid:")) {
					images[images.length] = {
						cid : elem.src,
						src : elem.src
					};
				}

				if (elem) {
					// just remove the attribute used by the old editor
					elem.removeAttribute("data-inline");

					if (elem.hasAttribute("data-name")) {
						data_names[data_names.length] = {
							elem : elem,
							name : elem.getAttribute("data-name")
						};

						elem.removeAttribute("data-name");
					}
				}
			}

			var backgrounds = document.querySelectorAll("[background]");
			for (ii = 0; ii < backgrounds.length; ii++) {
				var elem = backgrounds[ii];
				var src = elem ? elem.getAttribute("background").toLowerCase() : "";

				if (elem && (
				    src.startsWith("data:") ||
				    src.startsWith("file://") ||
				    src.startsWith("evo-file://"))) {
					var bkg = elem.getAttribute("background");

					for (jj = 0; jj < bkg_elems.length; jj++) {
						if (bkg == bkg_elems[jj].orig_src) {
							bkg_elems[jj].subelems[bkg_elems[jj].subelems.length] = elem;
							elem.setAttribute("background", bkg_elems[jj].cid);
							break;
						}
					}

					if (jj >= bkg_elems.length) {
						var bkg_obj = {
							subelems : [ elem ],
							cid : "cid:" + cid_uid_prefix + "-" + bkg_elems.length,
							orig_src : bkg
						};

						// re-read, because it could change
						if (elem.getAttribute("background").toLowerCase().startsWith("cid:"))
							bkg_obj.cid = elem.getAttribute("background");

						bkg_elems[bkg_elems.length] = bkg_obj;
						images[images.length] = {
							cid : bkg_obj.cid,
							src : elem.getAttribute("background")
						};
						elem.setAttribute("background", bkg_obj.cid);

						if (elem.hasAttribute("data-name"))
							images[images.length - 1].name = elem.getAttribute("data-name");
					}
				} else if (elem && src.startsWith("cid:")) {
					images[images.length] = {
						cid : elem.getAttribute("background"),
						src : elem.getAttribute("background")
					};
				}

				if (elem) {
					if (elem.hasAttribute("data-name")) {
						data_names[data_names.length] = {
							elem : elem,
							name : elem.getAttribute("data-name")
						};

						elem.removeAttribute("data-name");
					}
				}
			}

			if (images.length)
				content_data["images"] = images;
		}

		// Draft should have replaced images as well
		if ((flags & EvoEditor.E_CONTENT_EDITOR_GET_RAW_DRAFT) != 0) {
			document.head.setAttribute("x-evo-selection", EvoSelection.ToString(EvoSelection.Store(document)));
			try {
				content_data["raw-draft"] = document.documentElement.innerHTML;
			} finally {
				document.head.removeAttribute("x-evo-selection");
			}
		}

		if ((flags & EvoEditor.E_CONTENT_EDITOR_GET_TO_SEND_HTML) != 0)
			content_data["to-send-html"] = EvoEditor.convertHtmlToSend();

		if ((flags & EvoEditor.	E_CONTENT_EDITOR_GET_TO_SEND_PLAIN) != 0) {
			content_data["to-send-plain"] = EvoConvert.ToPlainText(document.body, EvoEditor.NORMAL_PARAGRAPH_WIDTH);
		}
	} finally {
		try {
			for (ii = 0; ii < img_elems.length; ii++) {
				var img_obj = img_elems[ii];

				for (jj = 0; jj < img_obj.subelems.length; jj++) {
					img_obj.subelems[jj].src = img_obj.orig_src;
				}
			}

			for (ii = 0; ii < data_names.length; ii++) {
				data_names[ii].elem.setAttribute("data-name", data_names[ii].name);
			}

			for (ii = 0; ii < bkg_elems.length; ii++) {
				var bkg_obj = bkg_elems[ii];

				for (jj = 0; jj < bkg_obj.subelems.length; jj++) {
					bkg_obj.subelems[jj].setAttribute("background", bkg_obj.orig_src);
				}
			}

			EvoEditor.RestoreCurrentElementAttr(currentElemsArray);
		} finally {
			EvoUndoRedo.Enable();
		}
	}

	// the above changes can cause change of the scroll offset, thus restore it
	window.scrollTo(scrollX, scrollY);

	return content_data;
}

EvoEditor.UpdateStyleSheet = function(id, css)
{
	var styles, ii, res = null;

	styles = document.head.getElementsByTagName("style");

	for (ii = 0; ii < styles.length; ii++) {
		if (styles[ii].id == id) {
			res = styles[ii].innerHTML;

			if (css)
				styles[ii].innerHTML = css;
			else
				document.head.removeChild(styles[ii]);

			return res;
		}
	}

	if (css) {
		var style;

		style = document.createElement("STYLE");
		style.id = id;
		style.innerHTML = css;
		document.head.append(style);
	}

	return res;
}

EvoEditor.UpdateThemeStyleSheet = function(css)
{
	return EvoEditor.UpdateStyleSheet("x-evo-theme-sheet", css);
}

EvoEditor.findSmileys = function(text, unicodeSmileys)
{
	/* Based on original use_pictograms() from GtkHTML */
	var emoticons_chars = [
		/*  0 */  "D",  "O",  ")",  "(",  "|",  "/",  "P",  "Q",  "*",  "!",
		/* 10 */  "S", null,  ":",  "-", null,  ":", null,  ":",  "-",  null,
		/* 20 */  ":", null,  ":",  ";",  "=",  "-", "\"", null,  ":",  ";",
		/* 30 */  "B", "\"",  "|", null,  ":",  "-",  "'", null,  ":",  "X",
		/* 40 */ null,  ":", null,  ":",  "-", null,  ":", null,  ":",  "-",
		/* 50 */ null,  ":", null,  ":",  "-", null,  ":", null,  ":",  "-",
		/* 60 */ null,  ":", null,  ":", null,  ":",  "-", null,  ":", null,
		/* 70 */  ":",  "-", null,  ":", null,  ":",  "-", null,  ":", null ];
	var emoticons_states = [
		/*  0 */  12,  17,  22,  34,  43,  48,  53,  58,  65,  70,
		/* 10 */  75,   0, -15,  15,   0, -15,   0, -17,  20,   0,
		/* 20 */ -17,   0, -14, -20, -14,  28,  63,   0, -14, -20,
		/* 30 */  -3,  63, -18,   0, -12,  38,  41,   0, -12,  -2,
		/* 40 */   0,  -4,   0, -10,  46,   0, -10,   0, -19,  51,
		/* 50 */   0, -19,   0, -11,  56,   0, -11,   0, -13,  61,
		/* 60 */   0, -13,   0,  -6,   0,  68,  -7,   0,  -7,   0,
		/* 70 */ -16,  73,   0, -16,   0, -21,  78,   0, -21,   0 ];
	var emoticons_icon_names = [
		"face-angel",
		"face-angry",
		"face-cool",
		"face-crying",
		"face-devilish",
		"face-embarrassed",
		"face-kiss",
		"face-laugh",		/* not used */
		"face-monkey",		/* not used */
		"face-plain",
		"face-raspberry",
		"face-sad",
		"face-sick",
		"face-smile",
		"face-smile-big",
		"face-smirk",
		"face-surprise",
		"face-tired",
		"face-uncertain",
		"face-wink",
		"face-worried"
	];
	var res = null, pos, state, start, uc;

	start = text.length - 1;

	if (start < 1)
		return res;

	pos = start;
	while (pos >= 0) {
		state = 0;
		while (pos >= 0) {
			uc = text[pos];
			var relative = 0;
			while (emoticons_chars[state + relative] != null) {
				if (emoticons_chars[state + relative] == uc) {
					break;
				}
				relative++;
			}
			state = emoticons_states[state + relative];
			/* 0 .. not found, -n .. found n-th */
			if (state <= 0)
				break;
			pos--;
		}

		/* Special case needed to recognize angel and devilish. */
		if (pos > 0 && state == -14) {
			uc = text[pos - 1];
			if (uc == 'O') {
				state = -1;
				pos--;
			} else if (uc == '>') {
				state = -5;
				pos--;
			}
		}

		if (state < 0) {
			if (pos > 0) {
				uc = text[pos - 1];

				if (uc != ' ') {
					return res;
				}
			}

			var obj = EvoEditor.lookupEmoticon(emoticons_icon_names[- state - 1], unicodeSmileys);

			if (obj) {
				obj.start = pos;
				obj.end = start + 1;

				if (!res)
					res = [];

				res[res.length] = obj;
			}

			pos--;
			start = pos;
		} else {
			break;
		}
	}

	return res;
}

EvoEditor.maybeUpdateParagraphWidth = function(topNode)
{
	if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
		var node = topNode, citeLevel = 0;

		while (node && node.tagName != "BODY") {
			if (node.tagName == "BLOCKQUOTE")
				citeLevel++;

			node = node.parentElement;
		}

		if (citeLevel * 2 < EvoEditor.NORMAL_PARAGRAPH_WIDTH) {
			topNode.style.width = (EvoEditor.NORMAL_PARAGRAPH_WIDTH - citeLevel * 2) + "ch";
		}
	}
}

EvoEditor.splitAtChild = function(parent, childNode)
{
	var newNode, node, next, ii;

	newNode = parent.ownerDocument.createElement(parent.tagName);

	for (ii = 0; ii < parent.attributes.length; ii++) {
		newNode.setAttribute(parent.attributes[ii].nodeName, parent.attributes[ii].nodeValue);
	}

	node = childNode;
	while (node) {
		next = node.nextSibling;
		newNode.appendChild(node);
		node = next;
	}

	parent.parentElement.insertBefore(newNode, parent.nextSibling);

	return newNode;
}

EvoEditor.hasElementWithTagNameAsParent = function(node, tagName)
{
	if (!node)
		return false;

	for (node = node.parentElement; node; node = node.parentElement) {
		if (node.tagName == tagName)
			return true;
	}

	return false;
}

EvoEditor.requoteNodeParagraph = function(node)
{
	while (node && node.tagName != "BODY" && !EvoEditor.IsBlockNode(node)) {
		node = node.parentElement;
	}

	if (!node || node.tagName == "BODY")
		return null;

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "requote", node, node,
		EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

	try {
		var blockquoteLevel = EvoEditor.getBlockquoteLevel(node);

		EvoEditor.quoteParagraph(node, blockquoteLevel, EvoEditor.NORMAL_PARAGRAPH_WIDTH - (2 * blockquoteLevel));
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "requote");
	}

	return node;
}

EvoEditor.replaceMatchWithNode = function(opType, node, match, newNode, canEmit, withUndo)
{
	if (withUndo) {
		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType, node.parentElement, node.parentElement,
			EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	}

	try {
		var selection = document.getSelection();
		var offset = selection.anchorOffset, updateSelection = selection.anchorNode === node, newAnchorNode;

		node.splitText(match.end);
		newAnchorNode = node.nextSibling;
		node.splitText(match.start);

		node = node.nextSibling;

		node.parentElement.insertBefore(newNode, node);
		if (newNode.tagName == "A")
			newNode.appendChild(node);
		else
			node.remove();

		if (updateSelection && newAnchorNode && offset - match.end >= 0)
			selection.setPosition(newAnchorNode, offset - match.end);
	} finally {
		if (withUndo) {
			EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType);

			if (canEmit) {
				EvoUndoRedo.GroupTopRecords(2);
				EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
				EvoEditor.EmitContentChanged();
			}
		}
	}

	return node;
}

EvoEditor.linkifyText = function(anchorNode, withUndo)
{
	if (!anchorNode)
		return false;

	var text = anchorNode.nodeValue, tmpNode;

	if (!text)
		return false;

	for (tmpNode = anchorNode; tmpNode && tmpNode.tagName != "BODY"; tmpNode = tmpNode.parentElement) {
		if (tmpNode.tagName == "A") {
			return false;
		}
	}

	var parts, ii;

	parts = EvoEditor.splitTextWithLinks(text);

	if (!parts)
		return false;

	if (withUndo) {
		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "magicLink", anchorNode.parentElement, anchorNode.parentElement,
			EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	}

	try {
		var selection = document.getSelection(), matchEnd = 0, insBefore, parent;
		var offset = selection.anchorOffset, updateSelection = selection.anchorNode === anchorNode, newAnchorNode = null;

		insBefore = anchorNode;
		parent = anchorNode.parentElement;

		for (ii = 0; ii < parts.length; ii++) {
			var part = parts[ii], node, isLast = ii + 1 >= parts.length;

			if (part.href) {
				node = document.createElement("A");
				node.href = part.href;
				node.innerText = part.text;
			} else if (isLast) {
				node = null;
				// it can be a space, which cannot be added after the element, thus workaround it this way
				newAnchorNode = anchorNode.splitText(matchEnd);
			} else {
				node = document.createTextNode(part.text);
			}

			if (node)
				parent.insertBefore(node, insBefore);

			if (!isLast) {
				matchEnd += part.text.length;
			} else if (node) {
				newAnchorNode = node;
			}
		}

		if (anchorNode)
			anchorNode.remove();

		if (updateSelection && newAnchorNode && offset - matchEnd >= 0)
			selection.setPosition(newAnchorNode, offset - matchEnd);
	} finally {
		if (withUndo) {
			EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "magicLink");

			EvoUndoRedo.GroupTopRecords(2);
			EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
			EvoEditor.EmitContentChanged();
		}
	}

	return true;
}

EvoEditor.maybeRemoveQuotationMark = function(node)
{
	if (!node || node.nodeType != node.ELEMENT_NODE || node.tagName != "SPAN" ||
	    node.className != "-x-evo-quoted")
		return false;

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "removeQuotationMark", node, node,
		EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML | EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);
	try {
		node.remove();
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "removeQuotationMark");
	}

	return true;
}

EvoEditor.removeEmptyElements = function(tagName)
{
	var nodes, node, ii, didRemove = 0;

	nodes = document.getElementsByTagName(tagName);

	for (ii = nodes.length - 1; ii >= 0; ii--) {
		node = nodes[ii];

		// more than one child element means it's not empty
		if (node.childElementCount > 1)
			continue;

		// the first element is not quotation mark
		if (node.firstElementChild && (node.firstElementChild.tagName != "SPAN" ||
		    node.firstElementChild.className != "-x-evo-quoted"))
			continue;

		// the text inside is not empty, possibly on either of the two sides of the quotation mark
		if ((node.firstChild && (node.firstChild.nodeType == node.TEXT_NODE && node.firstChild.nodeValue)) ||
		    (node.lastChild && !(node.lastChild === node.firstChild) && (node.lastChild.nodeType == node.TEXT_NODE && node.lastChild.nodeValue)))
			continue;

		// it's either completely empty or it contains only the quotation mark and nothing else
		didRemove++;

		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "removeEmptyElem::" + tagName, node.parentElement, node.parentElement,
			EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML | EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);
		try {
			node.remove();
		} finally {
			EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "removeEmptyElem::" + tagName);
		}
	}

	return didRemove;
}

EvoEditor.beforeInputCb = function(inputEvent)
{
	if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && inputEvent && (
	    inputEvent.inputType == "deleteContentForward" || inputEvent.inputType == "deleteContentBackward")) {
		var selection = document.getSelection();

		// workaround WebKit bug https://bugs.webkit.org/show_bug.cgi?id=209605
		if (selection.anchorNode && selection.anchorNode.nodeType == selection.anchorNode.ELEMENT_NODE &&
		    selection.isCollapsed && EvoEditor.IsBlockNode(selection.anchorNode) && selection.anchorNode.firstChild.tagName == "BR" &&
		    !selection.anchorNode.firstChild.nextSibling) {
			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_EVENT, inputEvent.inputType, selection.anchorNode, selection.anchorNode,
				EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML | EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);
			try {
				var next = selection.anchorNode.nextSibling;

				if (!next)
					next = selection.anchorNode.previousSibling;
				if (!next)
					next = selection.anchorNode.parentElement;

				selection.anchorNode.remove();

				selection.setPosition(next, 0);
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_EVENT, inputEvent.inputType);
			}

			inputEvent.stopImmediatePropagation();
			inputEvent.stopPropagation();
			inputEvent.preventDefault();

			return;
		}
	}

	if (EvoUndoRedo.disabled ||
	    !inputEvent ||
	    inputEvent.inputType != "insertText" ||
	    !inputEvent.data ||
	    inputEvent.data.length != 1 ||
	    inputEvent.data == " " ||
	    inputEvent.data == "\t")
		return;

	var selection = document.getSelection();

	// when writing at the end of the anchor, then write into the anchor, not out (WebKit writes out)
	if (!selection ||
	    !selection.isCollapsed ||
	    !selection.anchorNode ||
	    selection.anchorNode.nodeType != selection.anchorNode.TEXT_NODE ||
	    selection.anchorOffset != selection.anchorNode.nodeValue.length ||
	    !selection.anchorNode.parentElement ||
	    selection.anchorNode.parentElement.tagName != "A")
		return;

	var node = selection.anchorNode;

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_EVENT, "insertText", selection.anchorNode, selection.anchorNode,
		EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML | EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);

	try {
		node.nodeValue += inputEvent.data;
		selection.setPosition(node, node.nodeValue.length);

		if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT)
			node.parentElement.href = node.nodeValue;
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_EVENT, "insertText");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}

	// it will add the text, if anything breaks before it gets here
	inputEvent.stopImmediatePropagation();
	inputEvent.stopPropagation();
	inputEvent.preventDefault();
}

EvoEditor.AfterInputEvent = function(inputEvent, isWordDelim)
{
	var isInsertParagraph = inputEvent.inputType == "insertParagraph";
	var selection = document.getSelection();

	if (isInsertParagraph && selection.isCollapsed && selection.anchorNode && selection.anchorNode.tagName == "BODY") {
		document.execCommand("insertHTML", false, EvoEditor.emptyParagraphAsHtml());
		EvoUndoRedo.GroupTopRecords(2, "insertParagraph::withFormat");
		return;
	}

	// make sure there's always a DIV in the body (like after 'select all' followed by 'delete')
	if (!document.body.childNodes.length || (document.body.childNodes.length == 1 && document.body.childNodes[0].tagName == "BR")) {
		document.execCommand("insertHTML", false, EvoEditor.emptyParagraphAsHtml());
		EvoUndoRedo.GroupTopRecords(2, inputEvent.inputType + "::fillEmptyBody");
		return;
	}

	if (isInsertParagraph && selection.isCollapsed && selection.anchorNode && selection.anchorNode.tagName == "SPAN" &&
	    selection.anchorNode.children.length == 1 && selection.anchorNode.firstElementChild.tagName == "BR" &&
	    selection.anchorNode.parentElement.tagName == "DIV") {
		// new paragraph in UL/OL creates: <div><span style='white-space: normal;'><br></span><div>
		// thus avoid the <span />, which is not expected in the EvoEditor
		var node = selection.anchorNode;

		while (node.firstChild) {
			node.parentElement.insertBefore(node.firstChild, node);
		}

		selection.setPosition(node.parentElement, 0);
		node.remove();
	}

	if (isInsertParagraph && selection.isCollapsed && selection.anchorNode && selection.anchorNode.tagName == "DIV") {
		// for example when moving away from ul/ol, the newly created
		// paragraph can inherit styles from it, which is also negative text-indent
		selection.anchorNode.style.textIndent = "";
		EvoEditor.removeEmptyStyleAttribute(selection.anchorNode);
		EvoEditor.maybeUpdateParagraphWidth(selection.anchorNode);

		// it can be inherited, which is not desired when the user edits the content of it
		if (selection.anchorNode.className == "-x-evo-top-signature-spacer")
			selection.anchorNode.removeAttribute("class");
	}

	// inserting paragraph in BLOCKQUOTE creates a new BLOCKQUOTE without <DIV> inside it
	if (isInsertParagraph && selection.isCollapsed && selection.anchorNode && (selection.anchorNode.tagName == "BLOCKQUOTE" ||
	    (selection.anchorNode.nodeType == selection.anchorNode.TEXT_NODE && selection.anchorNode.parentElement &&
	     selection.anchorNode.parentElement.tagName == "BLOCKQUOTE"))) {
		var blockquoteNode = selection.anchorNode;

		if (blockquoteNode.nodeType == blockquoteNode.TEXT_NODE)
			blockquoteNode = blockquoteNode.parentElement;

		if (!blockquoteNode.firstChild || !EvoEditor.IsBlockNode(blockquoteNode.firstChild)) {
			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "blockquoteFix", blockquoteNode, blockquoteNode,
				EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

			try {
				var divNode = document.createElement("DIV");

				while (blockquoteNode.firstChild) {
					divNode.appendChild(blockquoteNode.firstChild);
				}

				blockquoteNode.appendChild(divNode);
				EvoEditor.maybeUpdateParagraphWidth(divNode);
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "blockquoteFix");
				EvoUndoRedo.GroupTopRecords(2, "insertParagraph::blockquoteFix");
				EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
				EvoEditor.EmitContentChanged();
			}
		}
	}

	// special editing of blockquotes
	if (selection.isCollapsed && (inputEvent.inputType.startsWith("insert") || inputEvent.inputType.startsWith("delete"))) {
		if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT && inputEvent.inputType.startsWith("delete")) {
			var didRemove = 0;

			didRemove += EvoEditor.removeEmptyElements("DIV");
			didRemove += EvoEditor.removeEmptyElements("PRE");

			if (didRemove)
				EvoUndoRedo.GroupTopRecords(didRemove + 1, inputEvent.inputType + "::removeEmptyElems");
		}

		if (EvoEditor.hasElementWithTagNameAsParent(selection.anchorNode, "BLOCKQUOTE")) {
			// insertParagraph should split the blockquote into two
			if (isInsertParagraph) {
				var node = selection.anchorNode, childNode = node, parent, removeNode = null;

				for (parent = node.parentElement; parent && parent.tagName != "BODY"; parent = parent.parentElement) {
					if (parent.tagName == "BLOCKQUOTE") {
						childNode = parent;
					}
				}

				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "blockquoteSplit", childNode, childNode,
					EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

				try {
					if (node.nodeType == node.ELEMENT_NODE && node.childNodes.length == 1 && node.firstChild.tagName == "BR")
						removeNode = node;
					else if (node.nodeType == node.ELEMENT_NODE && node.childNodes.length > 1 && node.firstChild.tagName == "BR")
						removeNode = node.firstChild;

					childNode = node;

					for (parent = node.parentElement; parent && parent.tagName != "BODY"; parent = parent.parentElement) {
						if (parent.tagName == "BLOCKQUOTE") {
							childNode = EvoEditor.splitAtChild(parent, childNode);
							parent = childNode;
						} else {
							childNode = parent;
						}
					}

					if (parent) {
						var divNode = document.createElement("DIV");
						divNode.appendChild(document.createElement("BR"));
						parent.insertBefore(divNode, childNode);
						document.getSelection().setPosition(divNode, 0);
						EvoEditor.maybeUpdateParagraphWidth(divNode);
					}

					while (removeNode && removeNode.tagName != "BODY") {
						node = removeNode.parentElement;
						node.removeChild(removeNode);

						if (node.childNodes.length)
							break;

						removeNode = node;
					}

					if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
						node = document.getSelection().anchorNode;

						if (node && node.nextElementSibling) {
							var blockquoteLevel = (node.nextElementSibling.tagName == "BLOCKQUOTE" ? 1 : 0);

							EvoEditor.removeQuoteMarks(node.nextElementSibling);
							EvoEditor.convertParagraphs(node.nextElementSibling, blockquoteLevel,
								EvoEditor.NORMAL_PARAGRAPH_WIDTH - (blockquoteLevel * 2), false);
						}

						if (node && node.previousElementSibling) {
							var blockquoteLevel = (node.previousElementSibling.tagName == "BLOCKQUOTE" ? 1 : 0);

							EvoEditor.removeQuoteMarks(node.previousElementSibling);
							EvoEditor.convertParagraphs(node.previousElementSibling, blockquoteLevel,
								EvoEditor.NORMAL_PARAGRAPH_WIDTH - (blockquoteLevel * 2), false);
						}
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "blockquoteSplit");
					EvoUndoRedo.GroupTopRecords(2, "insertParagraph::blockquoteSplit");
					EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
					EvoEditor.EmitContentChanged();
				}
			// insertLineBreak should re-quote text in the Plain Text mode
			} else if (inputEvent.inputType == "insertLineBreak") {
				if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
					var selNode = document.getSelection().anchorNode, node = selNode, parent;

					while (node && node.tagName != "BODY" && !EvoEditor.IsBlockNode(node)) {
						node = node.parentElement;
					}

					if (node && node.tagName != "BODY" && selNode.previousSibling && selNode.previousSibling.nodeValue == "\n") {
						EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "requote", node, node,
							EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

						try {
							var blockquoteLevel;

							// the "\n" is replaced with full paragraph
							selNode.parentElement.removeChild(selNode.previousSibling);

							parent = selNode.parentElement;

							var childNode = selNode;

							while (parent && parent.tagName != "BODY") {
								childNode = EvoEditor.splitAtChild(parent, childNode);

								if (childNode === node || EvoEditor.IsBlockNode(parent))
									break;

								parent = childNode.parentElement;
							}

							blockquoteLevel = EvoEditor.getBlockquoteLevel(parent);

							EvoEditor.quoteParagraph(childNode, blockquoteLevel, EvoEditor.NORMAL_PARAGRAPH_WIDTH - (2 * blockquoteLevel));

							document.getSelection().setPosition(childNode, 0);
						} finally {
							EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "requote");
							EvoUndoRedo.GroupTopRecords(2, "insertLineBreak::requote");
							EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
							EvoEditor.EmitContentChanged();
						}
					}
				}
			// it's an insert or delete in the blockquote, which means to recalculate where quotation marks should be
			} else if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
				var node = document.getSelection().anchorNode;

				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "requote::group");
				try {
					var selection = EvoSelection.Store(document);

					EvoEditor.removeEmptyElements("DIV");
					EvoEditor.removeEmptyElements("PRE");

					node = EvoEditor.requoteNodeParagraph(node);

					if (node && inputEvent.inputType.startsWith("delete")) {
						if (node.nextSiblingElement)
							EvoEditor.requoteNodeParagraph(node.nextSiblingElement);
						if (node.previousSiblingElement)
							EvoEditor.requoteNodeParagraph(node.previousSiblingElement);
					}

					EvoSelection.Restore(document, selection);
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, "requote::group");
					EvoUndoRedo.GroupTopRecords(2, inputEvent.inputType + "::requote");
					EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
					EvoEditor.EmitContentChanged();
				}
			}
		}
	}

	if ((!isInsertParagraph && inputEvent.inputType != "insertText") ||
	    (!(EvoEditor.MAGIC_LINKS && (isWordDelim || isInsertParagraph)) &&
	    !EvoEditor.MAGIC_SMILEYS)) {
		return;
	}

	if (!selection.isCollapsed || !selection.anchorNode)
		return;

	var anchorNode = selection.anchorNode, parentElem;

	if (anchorNode.nodeType != anchorNode.ELEMENT_NODE) {
		parentElem = anchorNode.parentElement;

		if (!parentElem)
			return;
	} else {
		parentElem = anchorNode;
	}

	if (isInsertParagraph) {
		parentElem = parentElem.previousElementSibling;

		if (!parentElem)
			return;

		anchorNode = parentElem.lastChild;

		if (!anchorNode || anchorNode.nodeType != anchorNode.TEXT_NODE)
			return;
	}

	if (!anchorNode.nodeValue)
		return;

	var canLinks;

	canLinks = EvoEditor.MAGIC_LINKS && (isWordDelim || isInsertParagraph);

	var text = anchorNode.nodeValue, covered = false;

	if (canLinks)
		covered = EvoEditor.linkifyText(anchorNode, true);

	if (!covered && EvoEditor.MAGIC_SMILEYS) {
		var matches;

		// the replace call below replaces &nbsp; (0xA0) with regular space
		matches = EvoEditor.findSmileys(text.replace(/ /g, " "), EvoEditor.UNICODE_SMILEYS);
		if (matches) {
			var ii, sz = matches.length, node, tmpElement = null;

			if (sz > 1)
				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "magicSmiley");

			try {
				// they are ordered from the end already
				for (ii = 0; ii < sz; ii++) {
					var match = matches[ii];

					if (!match.imageUri || EvoEditor.UNICODE_SMILEYS || EvoEditor.mode != EvoEditor.MODE_HTML) {
						node = document.createTextNode(match.text);
					} else {
						if (!tmpElement)
							tmpElement = document.createElement("SPAN");

						tmpElement.innerHTML = EvoEditor.createEmoticonHTML(match.text, match.imageUri, match.width, match.height);
						node = tmpElement.firstChild;
					}

					anchorNode = EvoEditor.replaceMatchWithNode("magicSmiley", anchorNode, match, node, sz == 1, true);
				}
			} finally {
				if (sz > 1) {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, "magicSmiley");
					EvoUndoRedo.GroupTopRecords(2);
					EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
					EvoEditor.EmitContentChanged();
				}
			}
		}
	}
}

EvoEditor.getParentElement = function(tagName, fromNode, canClimbUp)
{
	var node = fromNode;

	if (!node)
		node = document.getSelection().focusNode;

	if (!node)
		node = document.getSelection().anchorNode;

	while (node && node.nodeType != node.ELEMENT_NODE) {
		node = node.parentElement;
	}

	if (canClimbUp) {
		while (node && node.tagName != tagName) {
			node = node.parentElement;
		}
	}

	if (node && node.tagName == tagName)
		return node;

	return null;
}

EvoEditor.storePropertiesSelection = function()
{
	EvoEditor.propertiesSelection = EvoSelection.Store(document);
}

EvoEditor.restorePropertiesSelection = function()
{
	if (EvoEditor.propertiesSelection) {
		var selection = EvoEditor.propertiesSelection;

		EvoEditor.propertiesSelection = null;

		try {
			// Ignore any errors here
			EvoSelection.Restore(document, selection);
		} catch (exception) {
		}
	}
}

// returns an array with affected elements, which can be passed to EvoEditor.RestoreCurrentElementAttr()
EvoEditor.RemoveCurrentElementAttr = function()
{
	var nodes, ii, len, elems = [];

	nodes = document.querySelectorAll("[" + EvoEditor.CURRENT_ELEMENT_ATTR + "]");
	len = nodes ? nodes.length : 0;

	for (ii = 0; ii < len; ii++) {
		var elem = nodes[len - ii - 1];

		elems[elems.length] = elem;
		elem.removeAttribute(EvoEditor.CURRENT_ELEMENT_ATTR);
	}

	return elems;
}

EvoEditor.RestoreCurrentElementAttr = function(elemsArray)
{
	if (elemsArray) {
		var ii;

		for (ii = 0; ii < elemsArray.length; ii++) {
			elemsArray[ii].setAttribute(EvoEditor.CURRENT_ELEMENT_ATTR, "1");
		}
	}
}

EvoEditor.getCurrentElement = function()
{
	return document.querySelector("[" + EvoEditor.CURRENT_ELEMENT_ATTR + "]");
}

EvoEditor.setCurrentElement = function(element)
{
	EvoEditor.RemoveCurrentElementAttr();

	if (element)
		element.setAttribute(EvoEditor.CURRENT_ELEMENT_ATTR, "1");
}

EvoEditor.OnDialogOpen = function(name)
{
	EvoEditor.propertiesSelection = null;

	EvoEditor.RemoveCurrentElementAttr();

	var node = null;

	if (name == "link" || name == "cell" || name == "page") {
		EvoEditor.storePropertiesSelection();

		if (name == "cell") {
			var tdnode, thnode;

			tdnode = (EvoEditor.contextMenuNode && EvoEditor.contextMenuNode.tagName == "TD") ? EvoEditor.contextMenuNode : EvoEditor.getParentElement("TD", null, false);
			thnode = (EvoEditor.contextMenuNode && EvoEditor.contextMenuNode.tagName == "TH") ? EvoEditor.contextMenuNode : EvoEditor.getParentElement("TH", null, false);

			if (tdnode === EvoEditor.contextMenuNode) {
				node = tdnode;
			} else if (thnode === EvoEditor.contextMenuNode) {
				node = thnode;
			} else if (tdnode && thnode) {
				for (node = thnode; node; node = node.parentElement) {
					if (node === tdnode) {
						// TH is a child of TD
						node = thnode;
						break;
					}
				}

				if (!node)
					node = tdnode;
			} else {
				node = tdnode ? tdnode : thnode;
			}

			if (node)
				EvoEditor.setCurrentElement(node);
		}

		if (name == "cell" || name == "page")
			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "Dialog::" + name);
	} else if (name == "hrule" || name == "image" || name == "table") {
		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "Dialog::" + name);

		if (name == "hrule") {
			node = (EvoEditor.contextMenuNode && EvoEditor.contextMenuNode.tagName == "HR") ? EvoEditor.contextMenuNode : EvoEditor.getParentElement("HR", null, false);
		} else if (name == "image") {
			node = (EvoEditor.contextMenuNode && EvoEditor.contextMenuNode.tagName == "IMG") ? EvoEditor.contextMenuNode : EvoEditor.getParentElement("IMG", null, false);
		} else if (name == "table") {
			node = (EvoEditor.contextMenuNode && EvoEditor.contextMenuNode.tagName == "TABLE") ? EvoEditor.contextMenuNode : EvoEditor.getParentElement("TABLE", null, true);
		}

		if (node) {
			EvoEditor.setCurrentElement(node);
		} else {
			if (name == "hrule")
				EvoEditor.InsertHTML("CreateHRule", "<HR " + EvoEditor.CURRENT_ELEMENT_ATTR + "=\"1\">");
			else if (name == "image")
				EvoEditor.InsertHTML("CreateImage", "<IMG " + EvoEditor.CURRENT_ELEMENT_ATTR + "=\"1\">");
			else if (name == "table")
				EvoEditor.InsertHTML("CreateTable", "<TABLE " + EvoEditor.CURRENT_ELEMENT_ATTR + "=\"1\"></TABLE>");
		}
	}

	node = EvoEditor.getCurrentElement();

	if (node && name != "table" && name != "cell" && name != "image") {
		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_EVENT, "Dialog::" + name + "::event", node, node,
			EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
		EvoUndoRedo.Disable();
	}
}

EvoEditor.OnDialogClose = function(name)
{
	if (name == "link" || name == "cell")
		EvoEditor.restorePropertiesSelection();
	else
		EvoEditor.propertiesSelection = null;

	EvoEditor.contextMenuNode = null;

	var node = EvoEditor.getCurrentElement();

	EvoEditor.RemoveCurrentElementAttr();

	if (node && name != "table" && name != "cell" && name != "image") {
		EvoUndoRedo.Enable();
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_EVENT, "Dialog::" + name + "::event");
	}

	if (name == "hrule" || name == "image" || name == "table" || name == "cell" || name == "page")
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, "Dialog::" + name);
}

EvoEditor.applySetAttribute = function(record, isUndo)
{
	var element = EvoSelection.FindElementByPath(document.body, record.path);

	if (!element)
		throw "EvoEditor.applySetAttribute: Path not found";

	var value;

	if (isUndo)
		value = record.beforeValue;
	else
		value = record.afterValue;

	if (value == null)
		element.removeAttribute(record.attrName);
	else
		element.setAttribute(record.attrName, value);
}

EvoEditor.setAttributeWithUndoRedo = function(opTypePrefix, element, name, value)
{
	if (!element)
		return false;

	if ((value == null && !element.hasAttribute(name)) ||
	    (value != null && value == element.getAttribute(name)))
		return false;

	var record = EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opTypePrefix + "::" + name, element, element, EvoEditor.CLAIM_CONTENT_FLAG_NONE);

	try {
		if (record) {
			record.path = EvoSelection.GetChildPath(document.body, element);
			record.attrName = name;
			record.beforeValue = element.hasAttribute(name) ? element.getAttribute(name) : null;
			record.afterValue = value;
			record.apply = EvoEditor.applySetAttribute;
		}

		if (value == null) {
			element.removeAttribute(name);
		} else {
			element.setAttribute(name, value);
		}

		if (record && record.beforeValue == record.afterValue) {
			record.ignore = true;
		}
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opTypePrefix + "::" + name);

		if (!EvoUndoRedo.IsRecording()) {
			EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
			EvoEditor.EmitContentChanged();
		}
	}

	return true;
}

EvoEditor.addElementWithUndoRedo = function(opType, tagName, fillNodeFunc, parent, insertBefore, contentArray)
{
	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType, parent, parent, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	try {
		var selectionUpdater = EvoSelection.CreateUpdaterObject(), node;

		node = document.createElement(tagName);

		if (fillNodeFunc)
			fillNodeFunc(node);

		parent.insertBefore(node, insertBefore);

		if (contentArray) {
			var ii;

			for (ii = 0; ii < contentArray.length; ii++) {
				node.append(contentArray[ii]);
			}
		}

		selectionUpdater.restore();
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType);
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.removeElementWithUndoRedo = function(opType, element)
{
	if (element) {
		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType, element.parentElement, element.parentElement, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
		try {
			var selectionUpdater = EvoSelection.CreateUpdaterObject(), firstChild;

			firstChild = element.firstChild;

			while (element.firstChild) {
				element.parentElement.insertBefore(element.firstChild, element);
			}

			selectionUpdater.beforeRemove(element);
			element.remove();
			selectionUpdater.afterRemove(firstChild);

			selectionUpdater.restore();
		} finally {
			EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType);
			EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
			EvoEditor.EmitContentChanged();
		}
	}
}

// 'value' can be 'null', to remove the attribute
EvoEditor.DialogUtilsSetAttribute = function(selector, name, value)
{
	var element;

	if (selector)
		element = document.querySelector(selector);
	else
		element = EvoEditor.getCurrentElement();

	if (element) {
		EvoEditor.setAttributeWithUndoRedo("DlgUtilsSetAttribute", element, name, value);
	}
}

EvoEditor.DialogUtilsGetAttribute = function(selector, name)
{
	var element;

	if (selector)
		element = document.querySelector(selector);
	else
		element = EvoEditor.getCurrentElement();

	if (element && element.hasAttribute(name))
		return element.getAttribute(name);

	return null;
}

EvoEditor.DialogUtilsHasAttribute = function(name)
{
	var element = EvoEditor.getCurrentElement();

	return element && element.hasAttribute(name);
}

EvoEditor.LinkGetProperties = function()
{
	var res = null, anchor = EvoEditor.getParentElement("A", null, false);

	if (anchor) {
		res = [];
		res["href"] = anchor.href;
		res["text"] = anchor.innerText;
	} else if (!document.getSelection().isCollapsed && document.getSelection().rangeCount > 0) {
		var range;

		range = document.getSelection().getRangeAt(0);

		if (range) {
			res = [];
			res["text"] = range.toString();
		}
	}

	return res;
}

EvoEditor.LinkSetProperties = function(href, text)
{
	// The properties dialog can discard selection, thus restore it before doing changes
	EvoEditor.restorePropertiesSelection();

	var anchor = EvoEditor.getParentElement("A", null, false);

	if (anchor && (anchor.href != href || anchor.innerText != text)) {
		EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "SetLinkValues", anchor, anchor, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
		try {
			if (anchor.href != href)
				anchor.href = href;
			if (anchor.innerText != text) {
				var selection = EvoSelection.Store(document);
				anchor.innerText = text;
				EvoSelection.Restore(document, selection);
			}
		} finally {
			EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "SetLinkValues");
			EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
			EvoEditor.EmitContentChanged();
		}
	} else if (!anchor && href != "" && text != "") {
		text = text.replace(/\&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
		href = href.replace(/\&/g, "&amp;").replace(/\"/g, "&quot;");

		EvoEditor.InsertHTML("CreateLink", "<A href=\"" + href + "\">" + text + "</A>");
	}
}

EvoEditor.Unlink = function()
{
	// The properties dialog can discard selection, thus restore it before doing changes
	EvoEditor.restorePropertiesSelection();

	var anchor = EvoEditor.getParentElement("A", null, false);

	EvoEditor.removeElementWithUndoRedo("Unlink", anchor);
}

EvoEditor.ReplaceImageSrc = function(selector, uri)
{
	if (!selector)
		selector = "#x-evo-dialog-current-element";

	var element = document.querySelector(selector);

	if (element) {
		if (uri) {
			var attrName;

			if (element.tagName == "IMG")
				attrName = "src";
			else
				attrName = "background";

			EvoEditor.setAttributeWithUndoRedo("ReplaceImageSrc", element, attrName, uri);
		} else {
			if (element.tagName == "IMG") {
				EvoEditor.removeElementWithUndoRedo("ReplaceImageSrc", element);
			} else {
				EvoEditor.setAttributeWithUndoRedo("ReplaceImageSrc", element, "background", null);
			}
		}
	}
}

EvoEditor.DialogUtilsSetImageUrl = function(href)
{
	var element = EvoEditor.getCurrentElement();

	if (element && element.tagName == "IMG") {
		var anchor = EvoEditor.getParentElement("A", element, true);

		if (anchor) {
			if (href && anchor.href != href) {
				EvoEditor.setAttributeWithUndoRedo("DialogUtilsSetImageUrl", anchor, "href", href);
			} else if (!href) {
				EvoEditor.removeElementWithUndoRedo("DialogUtilsSetImageUrl::unset", element);
			}
		} else if (href) {
			var fillHref = function(node) {
				node.href = href;
			};

			EvoEditor.addElementWithUndoRedo("DialogUtilsSetImageUrl", "A", fillHref, element.parentElement, element, [ element ]);
		}
	}
}

EvoEditor.DialogUtilsGetImageUrl = function()
{
	var element = EvoEditor.getCurrentElement(), res = null;

	if (element && element.tagName == "IMG") {
		var anchor = EvoEditor.getParentElement("A", element, true);

		if (anchor)
			res = anchor.href;
	}

	return res;
}

EvoEditor.DialogUtilsGetImageWidth = function(natural)
{
	var element = EvoEditor.getCurrentElement(), res = -1;

	if (element && element.tagName == "IMG") {
		if (natural)
			res = element.naturalWidth;
		else
			res = element.width;
	}

	return res;
}

EvoEditor.DialogUtilsGetImageHeight = function(natural)
{
	var element = EvoEditor.getCurrentElement(), res = -1;

	if (element && element.tagName == "IMG") {
		if (natural)
			res = element.naturalHeight;
		else
			res = element.height;
	}

	return res;
}

EvoEditor.dialogUtilsForeachTableScope = function(scope, traversar, opType)
{
	var cell = EvoEditor.getCurrentElement();

	if (!cell)
		throw "EvoEditor.dialogUtilsForeachTableScope: Current cell not found";

	traversar.selectionUpdater = EvoSelection.CreateUpdaterObject();

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, opType);

	try {
		var table = EvoEditor.getParentElement("TABLE", cell, true);

		var rowFunc = function(row, traversar) {
			var jj, length = row.cells.length;

			for (jj = 0; jj < length; jj++) {
				var cell = row.cells[length - jj - 1];

				if (cell && !traversar.exec(cell))
					return false;
			}

			return true;
		};

		if (scope == EvoEditor.E_CONTENT_EDITOR_SCOPE_CELL) {
			traversar.exec(cell);
		} else if (scope == EvoEditor.E_CONTENT_EDITOR_SCOPE_COLUMN) {
			if (table) {
				var length = table.rows.length, ii, cellIndex = cell.cellIndex;

				for (ii = 0; ii < length; ii++) {
					var row = table.rows[length - ii - 1];

					if (row && cellIndex < row.cells.length &&
					    !traversar.exec(row.cells[cellIndex]))
						break;
				}
			}
		} else if (scope == EvoEditor.E_CONTENT_EDITOR_SCOPE_ROW) {
			var row = EvoEditor.getParentElement("TR", cell, true);

			if (row)
				rowFunc(row, traversar);
		} else if (scope == EvoEditor.E_CONTENT_EDITOR_SCOPE_TABLE) {
			if (table) {
				var length = table.rows.length, ii;

				for (ii = 0; ii < length; ii++) {
					if (!rowFunc(table.rows[length - ii - 1], traversar))
						break;
				}
			}
		}

		try {
			traversar.selectionUpdater.restore();
		} catch (ex) {
		}

		EvoEditor.dialogUtilsTableEnsureCurrentElement(table);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, opType);
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);

		if (traversar.anyChanged)
			EvoEditor.EmitContentChanged();
	}

	traversar.selectionUpdater = null;
}

EvoEditor.DialogUtilsTableSetAttribute = function(scope, attrName, attrValue)
{
	var traversar = {
		attrName : attrName,
		attrValue : attrValue,
		anyChanged : false,

		exec : function(cell) {
			if (EvoEditor.setAttributeWithUndoRedo("", cell, this.attrName, this.attrValue))
				this.anyChanged = true;

			return true;
		}
	};

	EvoEditor.dialogUtilsForeachTableScope(scope, traversar, "TableSetAttribute::" + attrName);
}

EvoEditor.DialogUtilsTableGetCellIsHeader = function()
{
	var element = EvoEditor.getCurrentElement();

	return element && element.tagName == "TH";
}

EvoEditor.DialogUtilsTableSetHeader = function(scope, isHeader)
{
	var traversar = {
		isHeader : isHeader,
		selectionUpdater : null,
		anyChanged : false,

		exec : function(cell) {
			if ((!this.isHeader && cell.tagName == "TD") ||
			    (this.isHeader && cell.tagName == "TH"))
				return;

			this.anyChanged = true;

			var opType = this.isHeader ? "unsetheader" : "setheader";

			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType, cell, cell,
				EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
			try {
				var node = document.createElement(this.isHeader ? "TH" : "TD");

				while(cell.firstChild) {
					node.append(cell.firstChild);
				}

				var ii;

				for (ii = 0; ii < cell.attributes.length; ii++) {
					node.setAttribute(cell.attributes[ii].name, cell.attributes[ii].value);
				}

				cell.parentElement.insertBefore(node, cell);

				if (this.selectionUpdater)
					this.selectionUpdater.beforeRemove(cell);

				cell.remove();

				if (this.selectionUpdater)
					this.selectionUpdater.afterRemove(node);
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, opType);
			}

			return true;
		}
	};

	EvoEditor.dialogUtilsForeachTableScope(scope, traversar, "DialogUtilsTableSetHeader");
}

EvoEditor.DialogUtilsTableGetRowCount = function()
{
	var element = EvoEditor.getCurrentElement();

	if (!element)
		return 0;

	element = EvoEditor.getParentElement("TABLE", element, true);

	if (!element)
		return 0;

	return element.rows.length;
}

EvoEditor.dialogUtilsTableEnsureCurrentElement = function(table)
{
	if (table && !EvoEditor.getCurrentElement() && table.rows.length > 0) {
		EvoEditor.setCurrentElement(table.rows[0].cells.length > 0 ? table.rows[0].cells[0] : table);
	}
}

EvoEditor.DialogUtilsTableSetRowCount = function(rowCount)
{
	var currentElem = EvoEditor.getCurrentElement();

	if (!currentElem)
		return;

	var table = EvoEditor.getParentElement("TABLE", currentElem, true);

	if (!table || table.rows.length == rowCount)
		return;

	var selectionUpdater = EvoSelection.CreateUpdaterObject();

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "DialogUtilsTableSetRowCount", table, table, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

	try {
		var ii;

		if (table.rows.length < rowCount) {
			var jj, nCells = table.rows.length ? table.rows[0].cells.length : 1;

			for (ii = table.rows.length; ii < rowCount; ii++) {
				var row;

				row = table.insertRow(-1);

				for (jj = 0; jj < nCells; jj++) {
					row.insertCell(-1);
				}
			}
		} else if (table.rows.length > rowCount) {
			for (ii = table.rows.length; ii > rowCount; ii--) {
				table.deleteRow(ii - 1);
			}
		}

		try {
			// it can fail, due to removed rows
			selectionUpdater.restore();
		} catch (ex) {
		}

		EvoEditor.dialogUtilsTableEnsureCurrentElement(table);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "DialogUtilsTableSetRowCount");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.DialogUtilsTableGetColumnCount = function()
{
	var element = EvoEditor.getCurrentElement();

	if (!element)
		return 0;

	element = EvoEditor.getParentElement("TABLE", element, true);

	if (!element || !element.rows.length)
		return 0;

	return element.rows[0].cells.length;
}

EvoEditor.DialogUtilsTableSetColumnCount = function(columnCount)
{
	var currentElem = EvoEditor.getCurrentElement();

	if (!currentElem)
		return;

	var table = EvoEditor.getParentElement("TABLE", currentElem, true);

	if (!table || !table.rows.length || table.rows[0].cells.length == columnCount)
		return;

	var selectionUpdater = EvoSelection.CreateUpdaterObject();

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "DialogUtilsTableSetColumnCount", table, table, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);

	try {
		var ii, jj;

		for (jj = 0; jj < table.rows.length; jj++) {
			var row = table.rows[jj];

			if (row.cells.length < columnCount) {
				for (ii = row.cells.length; ii < columnCount; ii++) {
					row.insertCell(-1);
				}
			} else if (row.cells.length > columnCount) {
				for (ii = row.cells.length; ii > columnCount; ii--) {
					row.deleteCell(ii - 1);
				}
			}
		}

		try {
			// it can fail, due to removed columns
			selectionUpdater.restore();
		} catch (ex) {
		}

		EvoEditor.dialogUtilsTableEnsureCurrentElement(table);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "DialogUtilsTableSetColumnCount");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.DialogUtilsTableDeleteCellContent = function()
{
	var traversar = {
		anyChanged : false,

		exec : function(cell) {
			if (cell.firstChild) {
				this.anyChanged = true;

				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "subdeletecellcontent", cell, cell, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
				try {
					while (cell.firstChild) {
						if (this.selectionUpdater)
							this.selectionUpdater.beforeRemove(cell.firstChild);

						cell.removeChild(cell.firstChild);

						if (this.selectionUpdater)
							this.selectionUpdater.afterRemove(cell);
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "subdeletecellcontent");
				}
			}

			return true;
		}
	};

	EvoEditor.dialogUtilsForeachTableScope(EvoEditor.E_CONTENT_EDITOR_SCOPE_CELL, traversar, "TableDeleteCellContent");
}

EvoEditor.DialogUtilsTableDeleteColumn = function()
{
	var traversar = {
		anyChanged : false,

		exec : function(cell) {
			this.anyChanged = true;

			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "subdeletecolumn", cell, cell,
				EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
			try {
				if (this.selectionUpdater) {
					this.selectionUpdater.beforeRemove(cell);
					this.selectionUpdater.afterRemove(cell.nextElementSibling ? cell.nextElementSibling : cell.previousElementSibling);
				}

				cell.remove();
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "subdeletecolumn");
			}

			return true;
		}
	};

	EvoEditor.dialogUtilsForeachTableScope(EvoEditor.E_CONTENT_EDITOR_SCOPE_COLUMN, traversar, "TableDeleteColumn");
}

EvoEditor.DialogUtilsTableDeleteRow = function()
{
	var traversar = {
		anyChanged : false,

		exec : function(cell) {
			this.anyChanged = true;

			var row = cell.parentElement;

			if (!row)
				return false;

			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "subdeleterow", row, row,
				EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
			try {
				row.parentElement.deleteRow(row.rowIndex);
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "subdeleterow");
			}

			return false;
		}
	};

	EvoEditor.dialogUtilsForeachTableScope(EvoEditor.E_CONTENT_EDITOR_SCOPE_ROW, traversar, "TableDeleteColumn");
}

EvoEditor.DialogUtilsTableDelete = function()
{
	var element = EvoEditor.getCurrentElement();

	if (!element)
		return;

	element = EvoEditor.getParentElement("TABLE", element, true);

	if (!element)
		return;

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "TableDelete", element, element,
		EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	try {
		element.remove();
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "TableDelete");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

// 'what' can be "column" or "row",
// 'where' can be lower than 0 for before/above, higher than 0 for after/below
EvoEditor.DialogUtilsTableInsert = function(what, where)
{
	if (what != "column" && what != "row")
		throw "EvoEditor.DialogUtilsTableInsert: 'what' (" + what + ") can be only 'column' or 'row'";
	if (!where)
		throw "EvoEditor.DialogUtilsTableInsert: 'where' cannot be zero";

	var cell, table;

	cell = EvoEditor.getCurrentElement();

	if (!cell)
		return;

	table = EvoEditor.getParentElement("TABLE", cell, true);

	if (!table)
		return;

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "TableInsert::" + what, table, table,
		EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	try {
		var index, ii;

		if (what == "column") {
			index = cell.cellIndex;

			if (where > 0)
				index++;

			for (ii = 0; ii < table.rows.length; ii++) {
				table.rows[ii].insertCell(index <= table.rows[ii].cells.length ? index : -1);
			}
		} else { // what == "row"
			var row = EvoEditor.getParentElement("TR", cell, true);

			if (row) {
				index = row.rowIndex;

				if (where > 0)
					index++;

				row = table.insertRow(index <= table.rows.length ? index : -1);

				for (ii = 0; ii < table.rows[0].cells.length; ii++) {
					row.insertCell(-1);
				}
			}
		}
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "TableInsert::" + what);
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.GetCaretWord = function()
{
	if (document.getSelection().rangeCount < 1)
		return null;

	var range = document.getSelection().getRangeAt(0);

	if (!range)
		return null;

	range = range.cloneRange();
	range.expand("word");

	return range.toString();
}

EvoEditor.replaceSelectionWord = function(opType, expandWord, replacement)
{
	if (!expandWord && document.getSelection().isCollapsed)
		return;

	if (document.getSelection().rangeCount < 1)
		return;

	var range = document.getSelection().getRangeAt(0);

	if (!range)
		return;

	if (expandWord)
		range.expand("word");

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_EVENT, opType, null, null, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	try {
		var fragment = range.extractContents(), node;

		/* Get the text node to replace and leave other formatting nodes
		 * untouched (font color, boldness, ...). */
		fragment.normalize();

		for (node = fragment.firstChild; node && node.nodeType != node.TEXT_NODE; node = node.firstChild) {
			;
		}

		if (node && node.nodeType == node.TEXT_NODE && replacement) {
			var text;

			/* Replace the word */
			text = document.createTextNode(replacement);
			node.parentNode.replaceChild(text, node);

			/* Insert the word on current location. */
			range.insertNode(fragment);

			document.getSelection().setPosition(text, replacement ? replacement.length : 0);
		}
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_EVENT, opType);
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.ReplaceCaretWord = function(replacement)
{
	EvoEditor.replaceSelectionWord("ReplaceCaretWord", true, replacement);
}

EvoEditor.ReplaceSelection = function(replacement)
{
	EvoEditor.replaceSelectionWord("ReplaceSelection", false, replacement);
}

EvoEditor.SpellCheckContinue = function(fromCaret, directionNext)
{
	var selection, storedSelection = null;

	selection = document.getSelection();

	if (fromCaret) {
		storedSelection = EvoSelection.Store(document);
	} else {
		if (directionNext) {
			selection.modify("move", "left", "documentboundary");
		} else {
			selection.modify ("move", "right", "documentboundary");
			selection.modify ("extend", "backward", "word");
		}
	}

	var selectWord = function(selection, directionNext) {
		var anchorNode, anchorOffset;

		anchorNode = selection.anchorNode;
		anchorOffset = selection.anchorOffset;

		if (directionNext) {
			var focusNode, focusOffset;

			focusNode = selection.focusNode;
			focusOffset = selection.focusOffset;

			/* Jump _behind_ next word */
			selection.modify("move", "forward", "word");
			/* Jump before the word */
			selection.modify("move", "backward", "word");
			/* Select it */
			selection.modify("extend", "forward", "word");

			/* If the selection didn't change, then we have most probably reached the end of the document. */
			return !((anchorNode === selection.anchorNode) &&
				 (anchorOffset == selection.anchorOffset) &&
				 (focusNode === selection.focusNode) &&
				 (focusOffset == selection.focusOffset));
		} else {
			/* Jump on the beginning of current word */
			selection.modify("move", "backward", "word");
			/* Jump before previous word */
			selection.modify("move", "backward", "word");
			/* Select it */
			selection.modify("extend", "forward", "word");

			/* If the selection start didn't change, then we have most probably reached the beginning of the document. */
			return (!(anchorNode === selection.anchorNode)) ||
				(anchorOffset != selection.anchorOffset);
		}
	};

	while (selectWord(selection, directionNext)) {
		if (selection.rangeCount < 1)
			break;

		var range = selection.getRangeAt(0);

		if (!range)
			break;

		var word = range.toString();

		if (!EvoEditor.SpellCheckWord(word)) {
			/* Found misspelled word */
			return word;
		}
	}

	/* Restore the selection to contain the last misspelled word. This is
	 * reached only when we reach the beginning/end of the document */
	if (storedSelection)
		EvoSelection.Restore(document, storedSelection);

	return null;
}

EvoEditor.MoveSelectionToPoint = function(xx, yy, cancel_if_not_collapsed)
{
	if (!cancel_if_not_collapsed || document.getSelection().isCollapsed) {
		var range = document.caretRangeFromPoint(xx, yy);

		document.getSelection().removeAllRanges();
		document.getSelection().addRange(range);
	}
}

EvoEditor.createEmoticonHTML = function(text, imageUri, width, height)
{
	if (imageUri.toLowerCase().startsWith("file:"))
		imageUri = "evo-" + imageUri;

	if (imageUri && EvoEditor.mode == EvoEditor.MODE_HTML && !EvoEditor.UNICODE_SMILEYS)
		return "<img src=\"" + imageUri + "\" alt=\"" +
			text.replace(/\&/g, "&amp;").replace(/\"/g, "&quot;").replace(/\'/g, "&apos;") +
			"\" width=\"" + width + "px\" height=\"" + height + "px\">";

	return text;
}

EvoEditor.InsertEmoticon = function(text, imageUri, width, height)
{
	EvoEditor.InsertHTML("InsertEmoticon", EvoEditor.createEmoticonHTML(text, imageUri, width, height));
}

EvoEditor.InsertImage = function(imageUri, width, height)
{
	if (imageUri.toLowerCase().startsWith("file:"))
		imageUri = "evo-" + imageUri;

	var html = "<img src=\"" + imageUri + "\"";

	if (width > 0 && height > 0) {
		html += " width=\"" + width + "px\" height=\"" + height + "px\"";
	}

	html += ">";

	EvoEditor.InsertHTML("InsertImage", html);
}

EvoEditor.GetCurrentSignatureUid = function()
{
	var elem = document.querySelector(".-x-evo-signature[id]");

	if (elem)
		return elem.id;

	return "";
}

EvoEditor.removeUnwantedTags = function(parent)
{
	if (!parent)
		return;

	var child, next = null;

	for (child = parent.firstChild; child; child = next) {
		next = child.nextSibling;

		if (child.tagName == "STYLE" ||
		    child.tagName == "META")
			child.remove();
	}
}

EvoEditor.InsertSignature = function(content, isHTML, uid, fromMessage, checkChanged, ignoreNextChange, startBottom, topSignature, addDelimiter)
{
	var sigSpan, node, scrollX, scrollY;

	scrollX = window.scrollX;
	scrollY = window.scrollY;

	sigSpan = document.createElement("SPAN");
	sigSpan.className = "-x-evo-signature";
	sigSpan.id = uid;

	if (content) {
		if (isHTML && EvoEditor.mode != EvoEditor.MODE_HTML) {
			node = document.createElement("SPAN");
			node.innerHTML = content;

			EvoEditor.removeUnwantedTags(node);

			content = EvoConvert.ToPlainText(node, EvoEditor.NORMAL_PARAGRAPH_WIDTH);
			if (content != "") {
				content = "<PRE>" + content.replace(/\&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") + "</PRE>";
			}

			isHTML = false;
		}

		/* The signature dash convention ("-- \n") is specified
		 * in the "Son of RFC 1036", section 4.3.2.
		 * http://www.chemie.fu-berlin.de/outerspace/netnews/son-of-1036.html
		 */
		if (addDelimiter) {
			var found;

			if (isHTML) {
				found = content.substr(0, 8).toUpperCase().startsWith("-- <BR>") || content.match(/\n-- <BR>/i) != null;
			} else {
				found = content.startsWith("-- \n") || content.match(/\n-- \n/i) != null;
			}

			/* Skip the delimiter if the signature already has one. */
			if (!found) {
				/* Always use the HTML delimiter as we are never in anything
				 * like a strict plain text mode. */
				node = document.createElement("PRE");
				node.innerHTML = "-- <BR>";
				sigSpan.appendChild(node);
			}
		}

		sigSpan.insertAdjacentHTML("beforeend", content);

		node = sigSpan.querySelector("[data-evo-signature-plain-text-mode]");
		if (node)
			node.removeAttribute("[data-evo-signature-plain-text-mode]");

		node = sigSpan.querySelector("#-x-evo-selection-start-marker");
		if (node)
			node.remove();

		node = sigSpan.querySelector("#-x-evo-selection-end-marker");
		if (node)
			node.remove();

		EvoEditor.removeUnwantedTags(sigSpan);
	}

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "InsertSignature");
	try {
		var signatures, ii, done = false, useWrapper = null;

		signatures = document.getElementsByClassName("-x-evo-signature-wrapper");
		for (ii = signatures.length; ii-- && !done;) {
			var wrapper, signature;

			wrapper = signatures[ii];
			signature = wrapper.firstElementChild;

			/* When we are editing a message with signature, we need to unset the
			 * active signature id as if the signature in the message was edited
			 * by the user we would discard these changes. */
			if (fromMessage && content && signature) {
				if (checkChanged) {
					/* Normalize the signature that we want to insert as the one in the
					 * message already is normalized. */
					webkit_dom_node_normalize (WEBKIT_DOM_NODE (signature_to_insert));
					if (!webkit_dom_node_is_equal_node (WEBKIT_DOM_NODE (signature_to_insert), signature)) {
						/* Signature in the body is different than the one with the
						 * same id, so set the active signature to None and leave
						 * the signature that is in the body. */
						uid = "none";
						ignoreNextChange = true;
					}

					checkChanged = false;
					fromMessage = false;
				} else {
					/* Old messages will have the signature id in the name attribute, correct it. */
					if (signature.hasAttribute("name")) {
						id = signature.getAttribute("name");
						signature.id = id;
						signature.removeAttribute(name);
					} else {
						id = signature.id;
					}

					/* Keep the signature and check if is it the same
					 * as the signature in body or the user previously
					 * changed it. */
					checkChanged = true;
				}

				done = true;
			} else {
				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertSignature::old-changes", wrapper, wrapper,
					EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML | EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE);
				try {
					/* If the top signature was set we have to remove the newline
					 * that was inserted after it */
					if (topSignature) {
						node = document.querySelector(".-x-evo-top-signature-spacer");
						if (node && (!node.firstChild || !node.textContent ||
						    (node.childNodes.length == 1 && node.firstChild.tagName == "BR"))) {
							node.remove();
						}
					}

					/* Leave just one signature wrapper there as it will be reused. */
					if (ii) {
						wrapper.remove();
					} else {
						wrapper.removeChild(signature);
						useWrapper = wrapper;
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertSignature::old-changes");
				}
			}
		}

		if (!done) {
			if (useWrapper) {
				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertSignature::new-changes", useWrapper, useWrapper, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
				try {
					useWrapper.appendChild(sigSpan);

					/* Insert a spacer below the top signature */
					if (topSignature && content) {
						node = document.createElement("DIV");
						node.appendChild(document.createElement("BR"));
						node.className = "-x-evo-top-signature-spacer";

						document.body.insertBefore(node, useWrapper.nextSibling);

						EvoEditor.maybeUpdateParagraphWidth(node);
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertSignature::new-changes");
				}
			} else {
				useWrapper = document.createElement("DIV");
				useWrapper.className = "-x-evo-signature-wrapper";
				useWrapper.appendChild(sigSpan);

				EvoEditor.maybeUpdateParagraphWidth(useWrapper);

				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertSignature::new-changes", document.body, document.body, EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
				try {
					if (topSignature) {
						document.body.insertBefore(useWrapper, document.body.firstChild);

						node = document.createElement("DIV");
						node.appendChild(document.createElement("BR"));
						node.className = "-x-evo-top-signature-spacer";

						document.body.insertBefore(node, useWrapper.nextSibling);
					} else {
						document.body.appendChild(useWrapper);
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertSignature::new-changes");
				}
			}

			fromMessage = false;

			// Position the caret and scroll to it
			if (startBottom) {
				if (topSignature) {
					document.getSelection().setPosition(document.body.lastChild, 0);
				} else if (useWrapper.previousSibling) {
					document.getSelection().setPosition(useWrapper.previousSibling, 0);
				} else {
					document.getSelection().setPosition(useWrapper, 0);
				}
			} else {
				document.getSelection().setPosition(document.body.firstChild, 0);
			}

			node = document.getSelection().anchorNode;

			if (node) {
				if (node.nodeType != node.ELEMENT_NODE)
					node = node.parentElement;

				if (node && node.scrollIntoViewIfNeeded != undefined)
					node.scrollIntoViewIfNeeded();
			}
		}
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, "InsertSignature");
	}

	// the above changes can cause change of the scroll offset, thus restore it
	window.scrollTo(scrollX, scrollY);

	var res = [];

	res["fromMessage"] = fromMessage;
	res["checkChanged"] = checkChanged;
	res["ignoreNextChange"] = ignoreNextChange;
	res["newUid"] = uid;

	return res;
}

EvoEditor.isEmptyParagraph = function(node)
{
	if (!node || !EvoEditor.IsBlockNode(node))
		return false;

	return !node.children.length || (node.children.length == 1 && node.children[0].tagName == "BR");
}

// replaces current selection with the plain text or HTML, quoted or normal DIV
EvoEditor.InsertContent = function(text, isHTML, quote)
{
	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, "InsertContent");
	try {
		if (!document.getSelection().isCollapsed) {
			EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::sel-remove", null, null,
				EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
			try {
				document.getSelection().deleteFromDocument();
			} finally {
				EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::sel-remove");
			}
		}

		var wasPlain = !isHTML;
		var content = document.createElement(quote ? "BLOCKQUOTE" : "DIV");

		if (quote) {
			content.setAttribute("type", "cite");
		}

		if (isHTML) {
			content.innerHTML = text;

			// paste can contain <meta> elements, like the one with Content-Type, which can be removed
			while (content.firstElementChild && content.firstElementChild.tagName == "META") {
				content.removeChild(content.firstElementChild);
			}

			// remove comments at the beginning, like the Evolution's "<!-- text/html -->"
			while (content.firstChild && content.firstChild.nodeType == content.firstChild.COMMENT_NODE) {
				content.removeChild(content.firstChild);
			}

			// convert P into DIV
			var node = content.firstChild, next;

			while (node) {
				var removeNode = false;

				if (node.nodeType == node.ELEMENT_NODE && node.tagName == "P") {
					removeNode = true;

					var div = document.createElement("DIV");
					EvoEditor.moveNodeContent(node, div);
					node.parentElement.insertBefore(div, node.nextSibling);
				}

				next = EvoEditor.getNextNodeInHierarchy(node, content);

				if (removeNode)
					node.remove();

				node = next;
			}

			if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
				EvoEditor.convertParagraphs(content, quote ? 1 : 0, EvoEditor.NORMAL_PARAGRAPH_WIDTH, quote);
				content.innerText = EvoConvert.ToPlainText(content, EvoEditor.NORMAL_PARAGRAPH_WIDTH);
			} else {
				EvoEditor.convertParagraphs(content, quote ? 1 : 0, -1, quote);
			}
		} else {
			var lines = text.split("\n");

			if (lines.length == 1 || (lines.length == 2 && !lines[1])) {
				content.innerText = lines[0];
			} else {
				var ii, line, divNode;

				for (ii = 0; ii < lines.length; ii++) {
					line = lines[ii];
					divNode = document.createElement("DIV");

					content.appendChild(divNode);

					if (!line.length) {
						divNode.appendChild(document.createElement("BR"));
					} else {
						divNode.innerText = line;
					}
				}

				isHTML = true;
			}
		}

		if (EvoEditor.MAGIC_LINKS) {
			var node, next, covered = false;

			for (node = content.firstChild; node; node = next) {
				next = EvoEditor.getNextNodeInHierarchy(node, content);

				if (node.nodeType == node.TEXT_NODE)
					covered = EvoEditor.linkifyText(node, false) || covered;
			}

			if (covered && !isHTML) {
				EvoEditor.convertParagraphs(content, quote ? 1 : 0, EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT ? EvoEditor.NORMAL_PARAGRAPH_WIDTH : -1, quote);
				isHTML = true;
			}
		}

		if (quote) {
			if (!isHTML)
				EvoEditor.convertParagraphs(content, quote ? 1 : 0, EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT ? EvoEditor.NORMAL_PARAGRAPH_WIDTH : -1, quote);

			var anchorNode = document.getSelection().anchorNode, intoBody = false;

			if (!content.firstElementChild || (content.firstElementChild.tagName != "DIV" && content.firstElementChild.tagName != "P" &&
			    content.firstElementChild.tagName != "PRE")) {
				// enclose quoted text into DIV
				var node = document.createElement("DIV");

				while (content.firstChild) {
					node.appendChild(content.firstChild);
				}

				content.appendChild(node);
			}

			if (anchorNode) {
				var node, parentBlock = null;

				if (anchorNode.nodeType == anchorNode.ELEMENT_NODE) {
					node = anchorNode;
				} else {
					node = anchorNode.parentElement;
				}

				while (node && node.tagName != "BODY" && !EvoEditor.IsBlockNode(node)) {
					parentBlock = node;

					node = node.parentElement;
				}

				if (node && node.tagName != "BLOCKQUOTE")
					parentBlock = node;
				else if (!parentBlock)
					parentBlock = node;

				if (!parentBlock) {
					intoBody = true;
				} else {
					var willSplit = parentBlock.tagName == "DIV" || parentBlock.tagName == "P" || parentBlock.tagName == "PRE";

					EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::text", parentBlock, parentBlock,
						(willSplit ? EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE : 0) | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
					try {
						if (willSplit) {
							// need to split the content up to the parent block node
							if (anchorNode.nodeType == anchorNode.TEXT_NODE) {
								anchorNode.splitText(document.getSelection().anchorOffset);
							}

							var from = anchorNode.nextSibling, parent, nextFrom = null;

							parent = from ? from.parentElement : anchorNode.parentElement;

							if (!from && parent) {
								from = parent.nextElementSibling;
								nextFrom = from;
								parent = parent.parentElement;
							}

							while (parent && parent.tagName != "BODY") {
								nextFrom = null;

								if (from) {
									var clone;

									clone = from.parentElement.cloneNode(false);
									from.parentElement.parentElement.insertBefore(clone, from.parentElement.nextSibling);

									nextFrom = clone;

									while (from.nextSibling) {
										clone.appendChild(from.nextSibling);
									}

									clone.insertBefore(from, clone.firstChild);
								}

								if (parent === parentBlock.parentElement || (parent.parentElement && parent.parentElement.tagName == "BLOCKQUOTE")) {
									break;
								}

								from = nextFrom;
								parent = parent.parentElement;
							}
						}

						parentBlock.insertAdjacentElement("afterend", content);

						if (content.nextSibling)
							document.getSelection().setPosition(content.nextSibling, 0);
						else if (content.lastChild) {
							node = content.lastChild;

							while (node.lastChild)
								node = node.lastChild;

							document.getSelection().setPosition(node, node.nodeType == node.TEXT_NODE ? node.nodeValue.length : 0);
						} else
							document.getSelection().setPosition(content, 0);

						if (anchorNode.nodeType == anchorNode.ELEMENT_NODE && anchorNode.parentElement &&
						    EvoEditor.isEmptyParagraph(anchorNode)) {
							anchorNode.remove();
						} else {
							anchorNode = parentBlock.nextSibling.nextSibling;

							if (anchorNode && anchorNode.nodeType == anchorNode.ELEMENT_NODE && anchorNode.parentElement &&
							    EvoEditor.isEmptyParagraph(anchorNode)) {
								anchorNode.remove();
							}
						}
					} finally {
						EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::text");
					}
				}
			} else {
				intoBody = true;
			}

			if (intoBody) {
				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::text", document.body, document.body,
					EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
				try {
					document.body.insertAdjacentElement("afterbegin", content);
					EvoEditor.maybeUpdateParagraphWidth(content);
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::text");
				}
			}

			if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
				var ii;

				for (ii = 0; ii < content.children.length; ii++) {
					EvoEditor.requoteNodeParagraph(content.children[ii]);
				}
			}
		} else if (isHTML) {
			var list, ii;

			list = content.getElementsByTagName("BLOCKQUOTE");

			for (ii = 0; ii < list.length; ii++) {
				var node = list[ii];

				node.removeAttribute("class");
				node.removeAttribute("style");
			}

			var selection = document.getSelection();

			var useOuterHTML = !list.length &&
				!content.getElementsByTagName("DIV").length &&
				!content.getElementsByTagName("PRE").length;

			if (!useOuterHTML && selection.isCollapsed && selection.focusNode && EvoEditor.isEmptyParagraph(selection.focusNode)) {
				var node = selection.focusNode, lastNode = null;

				EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::replaceEmptyBlock", node, node,
					EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
				try {
					if (useOuterHTML) {
						lastNode = content;
						node.parentElement.insertBefore(content, node);
					} else {
						while (content.firstChild) {
							lastNode = content.firstChild;
							node.parentElement.insertBefore(content.firstChild, node);
						}
					}

					node.remove();

					if (lastNode) {
						while (lastNode.lastChild) {
							lastNode = lastNode.lastChild;
						}

						selection.setPosition(lastNode, lastNode.nodeType == lastNode.TEXT_NODE ? lastNode.nodeValue.length : 0);
					}
				} finally {
					EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "InsertContent::replaceEmptyBlock");
				}

				EvoEditor.correctParagraphsAfterInsertContent("InsertContent::inEmptyBlock");
			} else {
				useOuterHTML = useOuterHTML && !wasPlain;

				EvoEditor.InsertHTML("InsertContent::text", useOuterHTML ? content.outerHTML : content.innerHTML);
			}
		} else {
			EvoEditor.InsertText("InsertContent::text", content.innerText);
		}
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, "InsertContent");
		EvoEditor.maybeUpdateFormattingState(EvoEditor.FORCE_MAYBE);
		EvoEditor.EmitContentChanged();
	}
}

EvoEditor.splitPreTexts = function(node, isInPre, newNodes)
{
	if (!node)
		return;

	isInPre = isInPre || node.tagName == "PRE";

	var currPre = null, child, childIsPre, next;

	for (child = node.firstChild; child; child = next) {
		childIsPre = child.tagName == "PRE";
		next = child.nextSibling;

		if (childIsPre || child.tagName == "BLOCKQUOTE") {
			currPre = null;

			var list = [], ii, clone = null;

			EvoEditor.splitPreTexts(child, isInPre, list);

			for (ii = 0; ii < list.length; ii++) {
				if (childIsPre) {
					newNodes[newNodes.length] = list[ii];
				} else {
					if (!clone) {
						clone = child.cloneNode(false);
						newNodes[newNodes.length] = clone;
					}

					clone.appendChild(list[ii]);
				}
			}
		} else if (isInPre && child.nodeType == node.TEXT_NODE) {
			var text = child.nodeValue, pre, ii, lines;

			lines = text.split("\n");

			for (ii = 0; ii < lines.length; ii++) {
				var line = lines[ii].replace(/\r/g, "");

				// <pre> is shown as a block, thus adding a new line at the end behaves like two <br>-s
				if (!line && ii + 1 >= lines.length) {
					if (ii > 0)
						currPre = null;
					break;
				}

				if (ii == 0 && currPre) {
					if (line)
						currPre.appendChild(document.createTextNode(line));
					if (lines.length > 1)
						currPre = null;
					continue;
				}

				pre = document.createElement("PRE");

				if (line) {
					pre.innerText = line;
				} else {
					pre.appendChild(document.createElement("BR"));
				}

				currPre = pre;
				newNodes[newNodes.length] = pre;
			}
		} else if (currPre && child.tagName == "BR") {
			currPre = null;
		} else {
			child.remove();

			if (currPre) {
				currPre.appendChild(child);
			} else if (isInPre) {
				currPre = document.createElement("PRE");
				currPre.appendChild(child);

				newNodes[newNodes.length] = currPre;
			} else {
				newNodes[newNodes.length] = child;
			}
		}
	}
}

EvoEditor.traverseToRemoveInsignificantNewLines = function(parent)
{
	if (!parent)
		return;

	var child;

	for (child = parent.firstChild; child; child = child.nextSibling) {
		if (child.nodeType == child.TEXT_NODE) {
			var str = EvoConvert.RemoveInsignificantNewLines(child);

			if (str != child.nodeValue) {
				child.nodeValue = str;
			}
		} else if (child.firstChild) {
			EvoEditor.traverseToRemoveInsignificantNewLines(child);
		}
	}
}

EvoEditor.processLoadedContent = function()
{
	if (!document.body)
		return;

	var node, didCite, ii, list;

	if (!document.body.hasAttribute("data-evo-draft") && document.querySelector("PRE")) {
		var next, replacement;

		document.body.normalize();

		for (node = document.body.firstChild; node; node = next) {
			next = node.nextSibling;

			if (node.tagName == "PRE" || node.tagName == "BLOCKQUOTE") {
				list = [];

				EvoEditor.splitPreTexts(node, false, list);

				for (ii = 0; ii < list.length; ii++) {
					node.parentElement.insertBefore(list[ii], node);
				}

				node.remove();
			}
		}
	}

	// This is to have prepared the text nodes for plain text. The plain text mode
	// sets white-space for div-s to 'pre-wrap', which means the new lines are
	// significant, but before the conversion they are insignificant, because
	// the loaded content is regular HTML, not Plain Text-like HTML.
	EvoEditor.UpdateStyleSheet("processLoadedContent", "body div { white-space: normal; }");
	try {
		EvoEditor.traverseToRemoveInsignificantNewLines(document.body);
	} finally {
		EvoEditor.UpdateStyleSheet("processLoadedContent", null);
	}

	node = document.querySelector("SPAN.-x-evo-cite-body");

	didCite = node;

	if (node)
		node.remove();

	if (didCite) {
		didCite = document.createElement("BLOCKQUOTE");
		didCite.setAttribute("type", "cite");

		while (document.body.firstChild) {
			didCite.appendChild(document.body.firstChild);
		}

		var next;

		// Evolution builds HTML with insignificant "\n", thus remove them first
		for (node = didCite.firstChild; node; node = next) {
			next = EvoEditor.getNextNodeInHierarchy(node, didCite);

			if (node.nodeType == node.TEXT_NODE && node.nodeValue && node.nodeValue.charAt(0) == '\n' && (
			    (node.previousSibling && EvoEditor.IsBlockNode(node.previousSibling)) ||
			    (!node.previousSibling && node.parentElement.tagName == "BLOCKQUOTE" && !(node.parentElement === didCite)))) {
				node.nodeValue = node.nodeValue.substr(1);
			}
		}

		document.body.appendChild(didCite);
	}

	list = document.querySelectorAll("STYLE[id]");

	for (ii = list.length - 1; ii >= 0; ii--) {
		node = list[ii];

		if (node.id && node.id.startsWith("-x-evo-"))
			node.remove();
	}

	list = document.querySelectorAll("DIV[data-headers]");

	for (ii = list.length - 1; ii >= 0; ii--) {
		node = list[ii];

		node.removeAttribute("data-headers");

		document.body.insertAdjacentElement("afterbegin", node);
	}

	list = document.querySelectorAll("SPAN.-x-evo-to-body[data-credits]");

	for (ii = list.length - 1; ii >= 0; ii--) {
		node = list[ii];

		var credits = node.getAttribute("data-credits");
		if (credits) {
			var elem;

			elem = document.createElement("DIV");
			elem.innerText = credits;

			document.body.insertAdjacentElement("afterbegin", elem);
			EvoEditor.maybeUpdateParagraphWidth(elem);
		}

		node.remove();
	}

	list = document.querySelectorAll(".-x-evo-paragraph");

	for (ii = list.length - 1; ii >= 0; ii--) {
		node = list[ii];
		node.removeAttribute("class");
	}

	list = document.querySelectorAll("[data-evo-paragraph]");

	for (ii = list.length - 1; ii >= 0; ii--) {
		list[ii].removeAttribute("data-evo-paragraph");
	}

	// require blocks under BLOCKQUOTE and style them properly
	list = document.getElementsByTagName("BLOCKQUOTE");

	for (ii = list.length - 1; ii >= 0; ii--) {
		var blockquoteNode = list[ii], addingTo = null, next;

		for (node = blockquoteNode.firstChild; node; node = next) {
			next = node.nextSibling;

			if (!EvoEditor.IsBlockNode(node) && (node.nodeType == node.ELEMENT_NODE || (node.nodeValue && node.nodeValue != "\n" && node.nodeValue != "\r\n"))) {
				if (!addingTo) {
					addingTo = document.createElement(EvoEditor.hasElementWithTagNameAsParent(node, "PRE") ? "PRE" : "DIV");
					blockquoteNode.insertBefore(addingTo, node);
					EvoEditor.maybeUpdateParagraphWidth(addingTo);
				}

				addingTo.appendChild(node);
			} else {
				addingTo = null;
			}
		}

		blockquoteNode.removeAttribute("class");
		blockquoteNode.removeAttribute("style");
	}

	if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT) {
		EvoEditor.convertParagraphs(document.body, 0, EvoEditor.NORMAL_PARAGRAPH_WIDTH, didCite);

		if (EvoEditor.MAGIC_LINKS) {
			var next;

			for (node = document.body.firstChild; node; node = next) {
				next = EvoEditor.getNextNodeInHierarchy(node, null);

				if (node.nodeType == node.TEXT_NODE)
					EvoEditor.linkifyText(node, false);
			}
		}
	}

	// remove comments at the beginning, like the Evolution's "<!-- text/html -->"
	while (document.documentElement.firstChild && document.documentElement.firstChild.nodeType == document.documentElement.firstChild.COMMENT_NODE) {
		document.documentElement.removeChild(document.documentElement.firstChild);
	}

	document.body.removeAttribute("data-evo-draft");
	document.body.removeAttribute("data-evo-plain-text");
	document.body.removeAttribute("spellcheck");

	list = document.querySelectorAll("[id=-x-evo-input-start]");

	for (ii = list.length - 1; ii >= 0; ii--) {
		node = list[ii];
		node.removeAttribute("id");

		document.getSelection().setPosition(node, 0);
		node.scrollIntoView();
	}

	if (EvoEditor.START_BOTTOM) {
		var node = document.createElement("DIV");

		node.appendChild(document.createElement("BR"));
		document.body.appendChild(node);
		EvoEditor.maybeUpdateParagraphWidth(node);

		document.getSelection().setPosition(node, 0);
		node.scrollIntoView();
	}
}

EvoEditor.LoadHTML = function(html)
{
	EvoUndoRedo.Disable();
	try {
		document.documentElement.innerHTML = html;

		EvoEditor.processLoadedContent();
		EvoEditor.initializeContent();
	} finally {
		EvoUndoRedo.Enable();
		EvoUndoRedo.Clear();
	}
}

EvoEditor.wrapParagraph = function(paragraphNode, maxLetters, currentPar, usedLetters, wasNestedElem)
{
	var child = paragraphNode.firstChild, nextChild, appendBR;

	while (child) {
		appendBR = false;

		if (child.nodeType == child.TEXT_NODE) {
			var text = child.nodeValue;

			// merge consecutive text nodes into one (similar to paragraphNode.normalize())
			while (child.nextSibling && child.nextSibling.nodeType == child.TEXT_NODE) {
				nextChild = child.nextSibling;
				text += nextChild.nodeValue;

				child.remove();

				child = nextChild;
			}

			while (text.length + usedLetters > maxLetters) {
				var spacePos = text.lastIndexOf(" ", maxLetters - usedLetters);

				if (spacePos < 0)
					spacePos = text.indexOf(" ");

				if (spacePos > 0 && (!usedLetters || usedLetters + spacePos <= maxLetters)) {
					var textNode = document.createTextNode(((usedLetters > 0 && !wasNestedElem) ? " " : "") +
						text.substr(0, spacePos));

					if (currentPar)
						currentPar.appendChild(textNode);
					else
						child.parentElement.insertBefore(textNode, child);

					text = text.substr(spacePos + 1);
				}

				if (currentPar)
					currentPar.appendChild(document.createElement("BR"));
				else
					child.parentElement.insertBefore(document.createElement("BR"), child);

				usedLetters = 0;

				if (spacePos == 0)
					text = text.substr(1);
				else if (spacePos < 0)
					break;
			}

			child.nodeValue = ((usedLetters > 0 && !wasNestedElem) ? " " : "") + text;
			usedLetters += ((usedLetters > 0 && !wasNestedElem) ? 1 : 0) + text.length;

			if (usedLetters > maxLetters)
				appendBR = true;

			wasNestedElem = false;
		} else if (child.tagName == "BR") {
			wasNestedElem = false;

			if (!child.nextSibling) {
				return -1;
			}

			if (child.nextSibling.tagName == "BR") {
				usedLetters = 0;

				if (currentPar) {
					var nextSibling = child.nextSibling;

					nextChild = child.nextSibling.nextSibling;

					currentPar.appendChild(child);

					if (usedLetters) {
						currentPar.appendChild(nextSibling);
					} else {
						nextSibling.remove();
					}

					child = nextChild;
					continue;
				}
			} else {
				nextChild = child.nextSibling;

				child.remove();

				child = nextChild;
				continue;
			}
		} else if (child.tagName == "IMG") {
			// just skip it, do not count it into the line length
			wasNestedElem = false;
		} else if (child.tagName == "B" ||
			   child.tagName == "I" ||
			   child.tagName == "U" ||
			   child.tagName == "S" ||
			   child.tagName == "SUB" ||
			   child.tagName == "SUP" ||
			   child.tagName == "FONT" ||
			   child.tagName == "SPAN" ||
			   child.tagName == "A") {
			usedLetters = EvoEditor.wrapParagraph(child, maxLetters, null, usedLetters, true);
			if (usedLetters == -1)
				usedLetters = 0;
			wasNestedElem = true;
		} else if (child.nodeType == child.ELEMENT_NODE) {
			// everything else works like a line stopper, with a new line added after it
			appendBR = true;
			wasNestedElem = false;
		}

		nextChild = child.nextSibling;

		if (currentPar)
			currentPar.appendChild(child);

		if (appendBR) {
			usedLetters = 0;

			if (nextChild) {
				if (currentPar)
					currentPar.appendChild(document.createElement("BR"));
				else
					nextChild.parentElement.insertBefore(document.createElement("BR"), nextChild);
			}
		}

		child = nextChild;
	}

	return usedLetters;
}

EvoEditor.WrapSelection = function()
{
	var nodeFrom, nodeTo;

	nodeFrom = EvoEditor.GetParentBlockNode(document.getSelection().anchorNode);
	nodeTo = EvoEditor.GetParentBlockNode(document.getSelection().focusNode);

	if (!nodeFrom || !nodeTo) {
		return;
	}

	if (nodeFrom != nodeTo) {
		// selection can go from top to bottom, but also from bottom to top; normalize the path order
		var commonParent = EvoEditor.GetCommonParent(nodeFrom, nodeTo, true), childFrom, childTo, ii, sz;

		childFrom = nodeFrom;
		while (childFrom && childFrom != commonParent && childFrom.parentElement != commonParent) {
			childFrom = childFrom.parentElement;
		}

		childTo = nodeTo;
		while (childTo && childTo != commonParent && childTo.parentElement != commonParent) {
			childTo = childTo.parentElement;
		}

		if (!childFrom || !childTo) {
			throw "EvoEditor.WrapSelection: Should not be reached (childFrom and childTo cannot be null)";
		}

		sz = commonParent.children.length;
		for (ii = 0; ii < sz; ii++) {
			if (commonParent.children[ii] === childFrom) {
				nodeFrom = childFrom;
				nodeTo = childTo;
				break;
			} else if (commonParent.children[ii] === childTo) {
				nodeFrom = childTo;
				nodeTo = childFrom;
				break;
			}
		}
	}

	EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "WrapSelection", nodeFrom, nodeTo, EvoEditor.CLAIM_CONTENT_FLAG_USE_PARENT_BLOCK_NODE | EvoEditor.CLAIM_CONTENT_FLAG_SAVE_HTML);
	try {
		var maxLetters, usedLetters, currentPar, lastParTagName = nodeFrom.tagName;

		maxLetters = EvoEditor.NORMAL_PARAGRAPH_WIDTH;
		usedLetters = 0;
		currentPar = null;

		while (nodeFrom) {
			EvoEditor.removeQuoteMarks(nodeFrom);

			if (lastParTagName != nodeFrom.tagName) {
				lastParTagName = nodeFrom.tagName;
				currentPar = null;
				usedLetters = 0;
			}

			if (nodeFrom.tagName == "DIV" || nodeFrom.tagName == "P" || nodeFrom.tagName == "PRE") {
				if (nodeFrom.childNodes.length == 1 && nodeFrom.childNodes[0].tagName == "BR") {
					currentPar = null;
					usedLetters = 0;
				} else {
					var blockquoteLevel = 0;

					if (EvoEditor.mode == EvoEditor.MODE_PLAIN_TEXT)
						blockquoteLevel = EvoEditor.getBlockquoteLevel(nodeFrom);

					usedLetters = EvoEditor.wrapParagraph(nodeFrom, maxLetters - (2 * blockquoteLevel), currentPar, usedLetters, false);

					if (blockquoteLevel)
						EvoEditor.requoteNodeParagraph(nodeFrom);

					if (usedLetters == -1) {
						currentPar = null;
						usedLetters = 0;
					} else if (!currentPar) {
						currentPar = nodeFrom;
					}
				}
			}

			// cannot break the cycle now, because want to delete the last empty paragraph
			var done = nodeFrom === nodeTo;

			if (!nodeFrom.childNodes.length) {
				var node = nodeFrom;

				nodeFrom = nodeFrom.nextSibling;

				if (node.parentElement)
					node.remove();
			} else {
				nodeFrom = nodeFrom.nextSibling;
			}

			if (done)
				break;
		}

		// Place the cursor at the end of the wrapped paragraph(s)
		if (currentPar)
			nodeTo = currentPar;

		while (nodeTo.lastChild) {
			nodeTo = nodeTo.lastChild;
		}

		document.getSelection().setPosition(nodeTo, nodeTo.nodeType == nodeTo.TEXT_NODE ? nodeTo.nodeValue.length : 0);
	} finally {
		EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_CUSTOM, "WrapSelection");
	}
}

EvoEditor.onContextMenu = function(event)
{
	var node = event.target;

	if (!node)
		node = document.getSelection().focusNode;
	if (!node)
		node = document.getSelection().anchorNode;

	EvoEditor.contextMenuNode = node;

	var nodeFlags = EvoEditor.E_CONTENT_EDITOR_NODE_UNKNOWN, res;

	while (node && node.tagName != "BODY") {
		if (node.tagName == "A")
			nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_ANCHOR;
		else if (node.tagName == "HR")
			nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_H_RULE;
		else if (node.tagName == "IMG")
			nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_IMAGE;
		else if (node.tagName == "TABLE")
			nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_TABLE;
		else if (node.tagName == "TD" || node.tagName == "TH")
			nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_TABLE_CELL;

		node = node.parentElement;
	}

	if (!nodeFlags && EvoEditor.contextMenuNode)
		nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_TEXT;

	if (document.getSelection().isCollapsed)
		nodeFlags |= EvoEditor.E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED;

	res = [];

	res["nodeFlags"] = nodeFlags;
	res["caretWord"] = EvoEditor.GetCaretWord();

	window.webkit.messageHandlers.contextMenuRequested.postMessage(res);
}

document.oncontextmenu = EvoEditor.onContextMenu;
document.onload = EvoEditor.initializeContent;

document.onselectionchange = function() {
	if (EvoEditor.checkInheritFontsOnChange) {
		EvoEditor.checkInheritFontsOnChange = false;
		EvoEditor.maybeReplaceInheritFonts();
	}

	EvoEditor.maybeUpdateFormattingState(EvoEditor.forceFormatStateUpdate ? EvoEditor.FORCE_YES : EvoEditor.FORCE_MAYBE);
	EvoEditor.forceFormatStateUpdate = false;

	window.webkit.messageHandlers.selectionChanged.postMessage(document.getSelection().isCollapsed);
};

EvoEditor.initializeContent();
