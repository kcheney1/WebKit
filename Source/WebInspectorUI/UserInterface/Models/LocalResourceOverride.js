/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.LocalResourceOverride = class LocalResourceOverride extends WI.Object
{
    constructor(url, type, localResource, {resourceErrorType, isCaseSensitive, isRegex, disabled} = {})
    {
        console.assert(url && typeof url === "string", url);
        console.assert(Object.values(WI.LocalResourceOverride.InterceptType).includes(type), type);
        console.assert(localResource instanceof WI.LocalResource, localResource);
        console.assert(!localResource.localResourceOverride, localResource);
        console.assert(!resourceErrorType || Object.values(WI.LocalResourceOverride.ResourceErrorType).includes(resourceErrorType), resourceErrorType);
        console.assert(isCaseSensitive === undefined || typeof isCaseSensitive === "boolean", isCaseSensitive);
        console.assert(isRegex === undefined || typeof isRegex === "boolean", isRegex);
        console.assert(disabled === undefined || typeof disabled === "boolean", disabled);

        super();

        this._url = url;
        this._urlComponents = null;
        this._type = type;
        this._localResource = localResource;
        this._resourceErrorType = resourceErrorType || WI.LocalResourceOverride.ResourceErrorType.General;
        this._isCaseSensitive = isCaseSensitive !== undefined ? isCaseSensitive : true;
        this._isRegex = isRegex !== undefined ? isRegex : false;
        this._disabled = disabled !== undefined ? disabled : false;

        this._localResource._localResourceOverride = this;
    }

    // Static

    static create(url, type, {requestURL, requestMethod, requestHeaders, requestData, responseMIMEType, responseContent, responseBase64Encoded, responseStatusCode, responseStatusText, responseHeaders, resourceErrorType, isCaseSensitive, isRegex, disabled} = {})
    {
        let localResource = new WI.LocalResource({
            request: {
                url: requestURL || url || "",
                method: requestMethod,
                headers: requestHeaders,
                data: requestData,
            },
            response: {
                headers: responseHeaders,
                mimeType: responseMIMEType,
                statusCode: responseStatusCode,
                statusText: responseStatusText,
                content: responseContent,
                base64Encoded: responseBase64Encoded,
            },
        });
        return new WI.LocalResourceOverride(url, type, localResource, {resourceErrorType, isCaseSensitive, isRegex, disabled});
    }

    static displayNameForNetworkStageOfType(type)
    {
        switch (type) {
        case WI.LocalResourceOverride.InterceptType.Block:
        case WI.LocalResourceOverride.InterceptType.Request:
            return WI.UIString("Request Override", "Request Override @ Local Override Network Stage", "Text indicating that the local override replaces the request of the network activity.");

        case WI.LocalResourceOverride.InterceptType.Response:
        case WI.LocalResourceOverride.InterceptType.ResponseSkippingNetwork:
            return WI.UIString("Response Override", "Response Override @ Local Override Network Stage", "Text indicating that the local override replaces the response of the network activity.");
        }

        console.assert(false, "Unknown type: ", type);
        return "";
    }

    static displayNameForType(type)
    {
        switch (type) {
        case WI.LocalResourceOverride.InterceptType.Block:
            return WI.UIString("Block", "Block @ Local Override Type", "Text indicating that the local override will always block the network activity.");

        case WI.LocalResourceOverride.InterceptType.Request:
            return WI.UIString("Request", "Request @ Local Override Type", "Text indicating that the local override intercepts the request phase of network activity.");

        case WI.LocalResourceOverride.InterceptType.Response:
            return WI.UIString("Response", "Response @ Local Override Type", "Text indicating that the local override intercepts the response phase of network activity.");

        case WI.LocalResourceOverride.InterceptType.ResponseSkippingNetwork:
            return WI.UIString("Response (skip network)", "Response (skip network) @ Local Override Type", "Text indicating that the local override will skip all network activity and instead immediately serve the response.");
        }

        console.assert(false, "Unknown type: ", type);
        return "";
    }

    static displayNameForResourceErrorType(resourceErrorType)
    {
        switch (resourceErrorType) {
        case WI.LocalResourceOverride.ResourceErrorType.AccessControl:
            return WI.UIString("Access Control", "Access Control @ Local Override Type", "Text indicating that the local override will block the network activity with an access error.");

        case WI.LocalResourceOverride.ResourceErrorType.Cancellation:
            return WI.UIString("Cancellation", "Cancellation @ Local Override Type", "Text indicating that the local override will block the network activity with a cancellation error.");

        case WI.LocalResourceOverride.ResourceErrorType.General:
            return WI.UIString("General", "General @ Local Override Type", "Text indicating that the local override will block the network activity with a general error.");

        case WI.LocalResourceOverride.ResourceErrorType.Timeout:
            return WI.UIString("Timeout", "Timeout @ Local Override Type", "Text indicating that the local override will block the network activity with an timeout error.");
        }

        console.assert(false, "Unknown resource error type: ", resourceErrorType);
        return "";
    }

    // Import / Export

    static fromJSON(json)
    {
        let {url, type, localResource: localResourceJSON, resourceErrorType, isCaseSensitive, isRegex, disabled} = json;

        let localResource = WI.LocalResource.fromJSON(localResourceJSON);

        // COMPATIBILITY (iOS 13.4): Network.interceptWithRequest/Network.interceptRequestWithResponse did not exist yet.
        url ??= localResource.url;
        type ??= WI.LocalResourceOverride.InterceptType.Response;

        return new WI.LocalResourceOverride(url, type, localResource, {resourceErrorType, isCaseSensitive, isRegex, disabled});
    }

    toJSON(key)
    {
        let json = {
            url: this._url,
            type: this._type,
            localResource: this._localResource.toJSON(key),
            isCaseSensitive: this._isCaseSensitive,
            isRegex: this._isRegex,
            disabled: this._disabled,
        };

        if (this._resourceErrorType)
            json.resourceErrorType = this._resourceErrorType;

        if (key === WI.ObjectStore.toJSONSymbol)
            json[WI.objectStores.localResourceOverrides.keyPath] = this._url;

        return json;
    }

    // Public

    get url() { return this._url; }
    get type() { return this._type; }
    get localResource() { return this._localResource; }
    get isCaseSensitive() { return this._isCaseSensitive; }
    get isRegex() { return this._isRegex; }

    get urlComponents()
    {
        if (!this._urlComponents)
            this._urlComponents = parseURL(this._url);
        return this._urlComponents;
    }

    get resourceErrorType()
    {
        return this._resourceErrorType;
    }

    set resourceErrorType(resourceErrorType)
    {
        console.assert(Object.values(WI.LocalResourceOverride.ResourceErrorType).includes(resourceErrorType), resourceErrorType);

        if (this._resourceErrorType === resourceErrorType)
            return;

        this._resourceErrorType = resourceErrorType;

        this.dispatchEventToListeners(WI.LocalResourceOverride.Event.ResourceErrorTypeChanged);
    }

    get disabled()
    {
        return this._disabled;
    }

    set disabled(disabled)
    {
        if (this._disabled === disabled)
            return;

        this._disabled = !!disabled;

        this.dispatchEventToListeners(WI.LocalResourceOverride.Event.DisabledChanged);
    }

    get displayName()
    {
        return this.displayURL();
    }

    displayURL({full} = {})
    {
        if (this._isRegex)
            return "/" + this._url + "/" + (!this._isCaseSensitive ? "i" : "");

        let displayName = full ? this._url : WI.displayNameForURL(this._url, this.urlComponents);
        if (!this._isCaseSensitive)
            displayName = WI.UIString("%s (Case Insensitive)", "%s (Case Insensitive) @ Local Override", "Label for case-insensitive URL match pattern of a local override.").format(displayName);
        return displayName;
    }

    get canMapToFile()
    {
        if (!WI.LocalResource.canMapToFile())
            return false;

        switch (this._type) {
        case WI.LocalResourceOverride.InterceptType.Block:
        case WI.LocalResourceOverride.InterceptType.Request:
            return false;

        case WI.LocalResourceOverride.InterceptType.Response:
        case WI.LocalResourceOverride.InterceptType.ResponseSkippingNetwork:
            return true;
        }

        console.assert(false, "not reached");
        return false;
    }

    matches(url)
    {
        if (this._isRegex) {
            let regex = new RegExp(this._url, !this._isCaseSensitive ? "i" : "");
            return regex.test(url);
        }

        if (!this._isCaseSensitive)
            return String(url).toLowerCase() === this._url.toLowerCase();

        return url === this._url;
    }

    equals(localResourceOverrideOrSerializedData)
    {
        console.assert(localResourceOverrideOrSerializedData instanceof WI.LocalResourceOverride || (localResourceOverrideOrSerializedData && typeof localResourceOverrideOrSerializedData === "object"), localResourceOverrideOrSerializedData);

        return localResourceOverrideOrSerializedData.url === this._url
            && localResourceOverrideOrSerializedData.isCaseSensitive === this._isCaseSensitive
            && localResourceOverrideOrSerializedData.isRegex === this._isRegex;
    }

    // Protected

    saveIdentityToCookie(cookie)
    {
        cookie["local-resource-override-url"] = this._url;
        cookie["local-resource-override-type"] = this._type;
        cookie["local-resource-override-is-case-sensitive"] = this._isCaseSensitive;
        cookie["local-resource-override-is-regex"] = this._isRegex;
        cookie["local-resource-override-disabled"] = this._disabled;
    }
};

WI.LocalResourceOverride.TypeIdentifier = "local-resource-override";

WI.LocalResourceOverride.InterceptType = {
    Block: "block",
    Request: "request",
    Response: "response",
    ResponseSkippingNetwork: "response-skipping-network",
};

WI.LocalResourceOverride.ResourceErrorType = {
    AccessControl: "AccessControl",
    Cancellation: "Cancellation",
    General: "General",
    Timeout: "Timeout",
};

WI.LocalResourceOverride.Event = {
    DisabledChanged: "local-resource-override-disabled-state-did-change",
    ResourceErrorTypeChanged: "local-resource-override-resource-error-type-changed",
};
