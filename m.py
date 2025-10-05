#!/usr/bin/env python3
"""
telegram_multi_lookup.py
Telegram bot that aggregates phone-number info from multiple public lookup providers.
Usage in Telegram chat:
    /num +919812345678
"""

import os
import asyncio
import logging
from typing import Dict, Any, Optional
import requests
from cachetools import TTLCache, cached
from dotenv import load_dotenv
from telegram import Update
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

load_dotenv()

# Config / env
TELEGRAM_TOKEN = os.environ.get("TELEGRAM_TOKEN")
NUMVERIFY_KEY = os.environ.get("NUMVERIFY_KEY")      # apilayer / numverify
TWILIO_SID = os.environ.get("TWILIO_SID")
TWILIO_AUTH = os.environ.get("TWILIO_AUTH")
WHITEPAGES_KEY = os.environ.get("WHITEPAGES_KEY")    # optional placeholder
CACHE_TTL = int(os.environ.get("CACHE_TTL", "3600"))

# Basic cache to avoid repeated paid requests
lookup_cache = TTLCache(maxsize=10000, ttl=CACHE_TTL)

# Logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ---------- Provider wrappers ----------

def call_numverify(number: str) -> Dict[str, Any]:
    """
    Uses Numverify / apilayer validate endpoint.
    Note: free tier may be HTTP and limited.
    """
    api_key = NUMVERIFY_KEY
    if not api_key:
        return {"provider": "numverify", "available": False, "error": "No NUMVERIFY_KEY configured"}
    url = "http://apilayer.net/api/validate"
    params = {"access_key": api_key, "number": number, "format": 1}
    try:
        r = requests.get(url, params=params, timeout=10)
        r.raise_for_status()
        data = r.json()
        data["provider"] = "numverify"
        data["available"] = True
        return data
    except Exception as e:
        logger.exception("Numverify error")
        return {"provider": "numverify", "available": False, "error": str(e)}

def call_twilio_lookup(number: str) -> Dict[str, Any]:
    """
    Calls Twilio Lookup API to fetch carrier and caller-name (CNAM) where supported.
    Requires TWILIO_SID and TWILIO_AUTH.
    """
    if not (TWILIO_SID and TWILIO_AUTH):
        return {"provider": "twilio", "available": False, "error": "No Twilio credentials configured"}
    # Twilio Lookup URL format
    url = f"https://lookups.twilio.com/v1/PhoneNumbers/{requests.utils.requote_uri(number)}"
    params = {"Type": ["carrier", "caller-name"]}  # requests will encode multiple params
    try:
        # Twilio requires HTTP Basic Auth
        r = requests.get(url, params={"Type": "carrier,caller-name"}, auth=(TWILIO_SID, TWILIO_AUTH), timeout=10)
        # Twilio returns 404 for invalid numbers or 200 w/ data
        if r.status_code == 200:
            data = r.json()
            data["provider"] = "twilio"
            data["available"] = True
            return data
        else:
            return {"provider": "twilio", "available": False, "status_code": r.status_code, "text": r.text}
    except Exception as e:
        logger.exception("Twilio lookup error")
        return {"provider": "twilio", "available": False, "error": str(e)}

def call_whitepages_placeholder(number: str) -> Dict[str, Any]:
    """
    Placeholder for Whitepages or other paid provider.
    Implement per your provider's API docs and return a dict.
    """
    if not WHITEPAGES_KEY:
        return {"provider": "whitepages", "available": False, "error": "No WHITEPAGES_KEY configured"}
    # Example: user should replace this block with real requests to Whitepages Pro or other vendor.
    # return requests.get("https://proapi.whitepages.com/...").json()
    return {"provider": "whitepages", "available": False, "error": "Placeholder - configure Whitepages integration"}

# ---------- Aggregation & formatting ----------

@cached(lookup_cache)
def aggregate_lookups(number: str) -> Dict[str, Any]:
    """
    Call available providers concurrently (synchronously inside this wrapper).
    Cached by number for CACHE_TTL seconds.
    """
    results = {}
    # Numverify (sync)
    try:
        results["numverify"] = call_numverify(number)
    except Exception as e:
        results["numverify"] = {"provider": "numverify", "available": False, "error": str(e)}

    # Twilio (sync)
    try:
        results["twilio"] = call_twilio_lookup(number)
    except Exception as e:
        results["twilio"] = {"provider": "twilio", "available": False, "error": str(e)}

    # Whitepages placeholder
    try:
        results["whitepages"] = call_whitepages_placeholder(number)
    except Exception as e:
        results["whitepages"] = {"provider": "whitepages", "available": False, "error": str(e)}

    # Merge best-effort:
    merged = {
        "queried_number": number,
        "providers": results,
        # Common consolidated info (try to pick best available fields)
        "international_format": None,
        "valid": None,
        "country_name": None,
        "country_code": None,
        "location": None,
        "carrier": None,
        "line_type": None,
        "caller_name": None,
    }

    # From Numverify
    nv = results.get("numverify") or {}
    if nv.get("available"):
        merged["international_format"] = nv.get("international_format") or merged["international_format"]
        merged["valid"] = nv.get("valid") if merged["valid"] is None else merged["valid"]
        merged["country_name"] = nv.get("country_name") or merged["country_name"]
        merged["country_code"] = nv.get("country_code") or merged["country_code"]
        merged["location"] = nv.get("location") or merged["location"]
        merged["carrier"] = nv.get("carrier") or merged["carrier"]
        merged["line_type"] = nv.get("line_type") or merged["line_type"]

    # From Twilio
    tw = results.get("twilio") or {}
    if tw.get("available"):
        # Twilio JSON structure: 'carrier' key, and possibly 'caller_name' key
        if tw.get("phone_number"):
            merged["international_format"] = tw.get("phone_number") or merged["international_format"]
        # Carrier info nested
        carrier = tw.get("carrier") or {}
        if carrier:
            merged["carrier"] = carrier.get("name") or merged["carrier"]
            merged["line_type"] = carrier.get("type") or merged["line_type"]
        # Caller-name
        cn = tw.get("caller_name")
        if cn:
            # Twilio caller_name example: {"caller_name":{"caller_name":"ABC CORP","error_code":null}}
            if isinstance(cn, dict):
                merged["caller_name"] = cn.get("caller_name") or merged["caller_name"]
            else:
                merged["caller_name"] = cn or merged["caller_name"]

    # Whitepages placeholder could augment merged if implemented

    return {"merged": merged, "raw": results}

def format_response(agg: Dict[str, Any]) -> str:
    m = agg["merged"]
    parts = []
    parts.append(f"🔎 Results for: {m.get('queried_number')}")
    parts.append("")
    parts.append(f"International: {m.get('international_format') or 'N/A'}")
    parts.append(f"Valid: {m.get('valid')}")
    parts.append(f"Country: {m.get('country_name') or 'N/A'} ({m.get('country_code') or 'N/A'})")
    parts.append(f"Location/Region: {m.get('location') or 'N/A'}")
    parts.append(f"Carrier: {m.get('carrier') or 'N/A'}")
    parts.append(f"Line type: {m.get('line_type') or 'N/A'}")
    parts.append(f"Caller name (CNAM): {m.get('caller_name') or 'N/A'}")
    parts.append("")
    parts.append("Providers summary:")
    for p, r in agg["raw"].items():
        ok = "OK" if r.get("available") else "NO"
        err = r.get("error") or r.get("text") or ""
        parts.append(f" - {p}: {ok} {('- ' + err) if err else ''}")
    parts.append("")
    parts.append("⚠️ Note: This bot shows public/API-provided info only. It cannot provide live GPS or covert personal data.")
    return "\n".join(parts)

# ---------- Telegram handlers ----------

async def start(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "Hi — send /num <phone_number_including_country_code>\nExample: /num +919812345678\nThis bot aggregates public info from multiple lookup providers."
    )

async def help_cmd(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text("Usage: /num +<countrycode><number>\nExample: /num +14155552671")

async def num_handler(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not context.args:
        await update.message.reply_text("Usage: /num <phone_number_with_country_code>\nExample: /num +919812345678")
        return

    number = context.args[0].strip()
    # Basic sanitization: allow + and digits only
    if not (number.startswith("+") and all(ch.isdigit() for ch in number[1:])):
        await update.message.reply_text("Please provide international format with + and digits only. Example: +919812345678")
        return

    await update.message.chat.send_action(action="typing")
    try:
        agg = await asyncio.get_event_loop().run_in_executor(None, aggregate_lookups, number)
        resp_text = format_response(agg)
    except Exception as e:
        logger.exception("Aggregate error")
        resp_text = f"Error during lookup: {e}"

    # Telegram has message length limits; split if needed
    if len(resp_text) > 4000:
        # send as file
        await update.message.reply_text("Result too long — sending as a text file.")
        await update.message.reply_text(resp_text[:4000])
    else:
        await update.message.reply_text(resp_text)

# ---------- Main ----------

def main():
    if not TELEGRAM_TOKEN:
        raise RuntimeError("TELEGRAM_TOKEN not set in environment")

    app = ApplicationBuilder().token(TELEGRAM_TOKEN).build()
    app.add_handler(CommandHandler("start", start))
    app.add_handler(CommandHandler("help", help_cmd))
    app.add_handler(CommandHandler("num", num_handler))

    logger.info("Starting bot...")
    app.run_polling()

if __name__ == "__main__":
    main()