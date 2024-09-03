use std::fmt::{Debug, Display, Formatter};
use serde_derive::{Deserialize, Serialize};
use urlencoding::{decode, encode};
use percent_encoding_rfc3986::{utf8_percent_encode, percent_decode, AsciiSet, CONTROLS, percent_decode_str};

#[derive(Serialize, Deserialize, Clone, Default)]
pub struct ClientString {
    _encoded_string_data: Option<String>,
}

impl Debug for ClientString {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.encoded().as_str())
    }
}

impl Display for ClientString {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self.plain() {
            Ok(plain) => {
                f.write_str(plain.as_str())
            }
            Err(_) => {
                f.write_str(self.encoded().as_str())
            }
        }
    }
}

impl ClientString {
    pub fn from_client(string: &str) -> Self {
        Self {
            _encoded_string_data: Some(Self::encode(string)),
        }
    }

    pub fn from_os_string(string: &std::ffi::OsStr) -> Self {
        Self {
            _encoded_string_data: Some(Self::encode(string.to_str().unwrap())),
        }
    }
    
    pub fn from_url(string: &str) -> Self {
        Self {
            _encoded_string_data: Some(string.to_string()),
        }
    }

    pub fn encode(decoded: &str) -> String {
        const FRAGMENT: &AsciiSet = &CONTROLS.add(b' ').add(b'"').add(b'<').add(b'>').add(b'`').add(b'&');
        utf8_percent_encode(decoded, FRAGMENT).to_string()
    }

    pub fn decode(encoded: &str) -> Result<String, failure::Error> {
        Ok(percent_decode_str(encoded).or(Err(failure::err_msg("Failed to decode input string")))?.decode_utf8()?.to_string())
    }

    pub fn plain(&self) -> Result<String, failure::Error> {
        match &self._encoded_string_data {
            None => { Ok(String::new()) }
            Some(string) => {
                Self::decode(string.as_str())
            }
        }
    }

    pub fn encoded(&self) -> String {
        match &self._encoded_string_data {
            None => { String::new() }
            Some(string) => { string.clone() }
        }
    }
}